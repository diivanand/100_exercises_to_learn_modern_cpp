// Solution -- 15.06 Pinned memory and copy/compute overlap
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cuda_runtime.h>

// From 14.02 and 14.03.
void check(cudaError_t status, const char* file, int line) {
  if (status != cudaSuccess) {
    throw std::runtime_error(std::string{cudaGetErrorName(status)} + ": " +
                             cudaGetErrorString(status) + " at " + file + ":" +
                             std::to_string(line));
  }
}
#define CUDA_CHECK(expr) check((expr), __FILE__, __LINE__)

template <typename T>
class DeviceBuffer {
public:
  DeviceBuffer() = default;
  explicit DeviceBuffer(std::size_t count) : count_(count) {
    if (count_ > 0) {
      CUDA_CHECK(cudaMalloc(&data_, count_ * sizeof(T)));
    }
  }
  ~DeviceBuffer() {
    cudaFree(data_);
  }
  DeviceBuffer(const DeviceBuffer&) = delete;
  DeviceBuffer& operator=(const DeviceBuffer&) = delete;
  DeviceBuffer(DeviceBuffer&& other) noexcept
      : data_(std::exchange(other.data_, nullptr)),
        count_(std::exchange(other.count_, 0)) {}
  DeviceBuffer& operator=(DeviceBuffer&& other) noexcept {
    if (this != &other) {
      cudaFree(data_);
      data_ = std::exchange(other.data_, nullptr);
      count_ = std::exchange(other.count_, 0);
    }
    return *this;
  }
  T* data() noexcept {
    return data_;
  }
  const T* data() const noexcept {
    return data_;
  }
  std::size_t size() const noexcept {
    return count_;
  }
  std::size_t bytes() const noexcept {
    return count_ * sizeof(T);
  }
  void upload(std::span<const T> host) {
    if (host.size() != count_) {
      throw std::invalid_argument("DeviceBuffer::upload: size mismatch");
    }
    CUDA_CHECK(cudaMemcpy(data_, host.data(), host.size_bytes(), cudaMemcpyHostToDevice));
  }
  void download(std::span<T> host) const {
    if (host.size() != count_) {
      throw std::invalid_argument("DeviceBuffer::download: size mismatch");
    }
    CUDA_CHECK(cudaMemcpy(host.data(), data_, host.size_bytes(), cudaMemcpyDeviceToHost));
  }

private:
  T* data_ = nullptr;
  std::size_t count_ = 0;
};

// Host memory the operating system may not page out, allocated with
// cudaHostAlloc. The GPU's DMA engine can read and write it directly, which
// is what makes cudaMemcpyAsync genuinely asynchronous. Same rule of five as
// DeviceBuffer; cudaFreeHost instead of cudaFree.
template <typename T>
class PinnedBuffer {
public:
  PinnedBuffer() = default;
  explicit PinnedBuffer(std::size_t count) : count_(count) {
    if (count_ > 0) {
      void* raw = nullptr;
      CUDA_CHECK(cudaHostAlloc(&raw, count_ * sizeof(T), cudaHostAllocDefault));
      data_ = static_cast<T*>(raw);
    }
  }
  ~PinnedBuffer() {
    cudaFreeHost(data_);
  }
  PinnedBuffer(const PinnedBuffer&) = delete;
  PinnedBuffer& operator=(const PinnedBuffer&) = delete;
  PinnedBuffer(PinnedBuffer&& other) noexcept
      : data_(std::exchange(other.data_, nullptr)),
        count_(std::exchange(other.count_, 0)) {}
  PinnedBuffer& operator=(PinnedBuffer&& other) noexcept {
    if (this != &other) {
      cudaFreeHost(data_);
      data_ = std::exchange(other.data_, nullptr);
      count_ = std::exchange(other.count_, 0);
    }
    return *this;
  }
  T* data() noexcept {
    return data_;
  }
  const T* data() const noexcept {
    return data_;
  }
  std::size_t size() const noexcept {
    return count_;
  }
  std::span<T> span() noexcept {
    return {data_, count_};
  }
  std::span<const T> span() const noexcept {
    return {data_, count_};
  }

private:
  T* data_ = nullptr;
  std::size_t count_ = 0;
};

class Stream {
public:
  Stream() {
    CUDA_CHECK(cudaStreamCreate(&stream_));
  }
  ~Stream() {
    cudaStreamDestroy(stream_);
  }
  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;
  cudaStream_t get() const noexcept {
    return stream_;
  }

private:
  cudaStream_t stream_{};
};

class Event {
public:
  Event() {
    CUDA_CHECK(cudaEventCreate(&event_));
  }
  ~Event() {
    cudaEventDestroy(event_);
  }
  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;
  Event(Event&& other) noexcept : event_(std::exchange(other.event_, nullptr)) {}
  Event& operator=(Event&& other) noexcept {
    if (this != &other) {
      cudaEventDestroy(event_);
      event_ = std::exchange(other.event_, nullptr);
    }
    return *this;
  }
  void record(cudaStream_t stream) {
    CUDA_CHECK(cudaEventRecord(event_, stream));
  }
  cudaEvent_t get() const noexcept {
    return event_;
  }

private:
  cudaEvent_t event_{};
};

// Milliseconds from `from` to `to`, by the GPU's clock. Both events must have
// completed; a cudaDeviceSynchronize() beforehand guarantees it.
float elapsed_ms(const Event& from, const Event& to) {
  float ms = 0.0F;
  CUDA_CHECK(cudaEventElapsedTime(&ms, from.get(), to.get()));
  return ms;
}

// One event per chunk after its upload and after its kernel. The test uses
// these to see whether uploads overlapped kernels; a profiler (17.02) would
// show the same thing as a picture.
struct Trace {
  explicit Trace(int chunks)
      : upload_done(static_cast<std::size_t>(chunks)),
        kernel_done(static_cast<std::size_t>(chunks)) {}
  std::vector<Event> upload_done;
  std::vector<Event> kernel_done;
};

// A deliberately slow kernel: it transforms its elements and then spins for a
// fixed number of clock cycles, so that "the kernel is running" is a period
// long enough for the copies around it to be seen overlapping it.
constexpr long long kSpinCycles = 20'000'000; // ~8 ms at 2.5 GHz
constexpr unsigned kBlock = 256;
constexpr unsigned kGrid = 512;

__global__ void slow_transform(const float* in, float* out, std::size_t n) {
  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    out[i] = in[i] * 2.0F + 1.0F;
  }
  const long long start = clock64();
  while (clock64() - start < kSpinCycles) {
  }
}

// Processes `input` in `chunks` equal pieces so that the upload of one chunk,
// the kernel of another and the download of a third are all in flight at
// once. Two "lanes" -- a stream and a set of buffers each -- alternate; a
// lane is reused only after everything queued on its stream has finished.
void process_chunks(std::span<const float> input, std::span<float> output, int chunks,
                    Trace& trace) {
  constexpr int kLanes = 2;
  const std::size_t chunk = input.size() / static_cast<std::size_t>(chunks);
  const std::size_t bytes = chunk * sizeof(float);

  Stream streams[kLanes];
  PinnedBuffer<float> staging_in[kLanes] = {PinnedBuffer<float>(chunk),
                                            PinnedBuffer<float>(chunk)};
  PinnedBuffer<float> staging_out[kLanes] = {PinnedBuffer<float>(chunk),
                                             PinnedBuffer<float>(chunk)};
  DeviceBuffer<float> d_in[kLanes] = {DeviceBuffer<float>(chunk),
                                      DeviceBuffer<float>(chunk)};
  DeviceBuffer<float> d_out[kLanes] = {DeviceBuffer<float>(chunk),
                                       DeviceBuffer<float>(chunk)};

  auto collect = [&](int k) {
    const int lane = k % kLanes;
    CUDA_CHECK(cudaStreamSynchronize(streams[lane].get()));
    std::copy_n(staging_out[lane].data(), chunk,
                output.data() + static_cast<std::size_t>(k) * chunk);
  };

  for (int k = 0; k < chunks; ++k) {
    const int lane = k % kLanes;
    const cudaStream_t stream = streams[lane].get();

    // This lane's previous chunk must be entirely finished -- including its
    // download into staging_out -- before its buffers are reused.
    if (k >= kLanes) {
      collect(k - kLanes);
    }

    // The host copies chunk k into pinned memory while the GPU works on
    // chunk k-1. Pageable memory cannot be handed to the DMA engine, so this
    // staging copy is the price of a truly asynchronous upload.
    std::copy_n(input.data() + static_cast<std::size_t>(k) * chunk, chunk,
                staging_in[lane].data());

    CUDA_CHECK(cudaMemcpyAsync(d_in[lane].data(), staging_in[lane].data(), bytes,
                               cudaMemcpyHostToDevice, stream));
    trace.upload_done[static_cast<std::size_t>(k)].record(stream);

    slow_transform<<<kGrid, kBlock, 0, stream>>>(d_in[lane].data(), d_out[lane].data(),
                                                 chunk);
    CUDA_CHECK(cudaGetLastError());
    trace.kernel_done[static_cast<std::size_t>(k)].record(stream);

    CUDA_CHECK(cudaMemcpyAsync(staging_out[lane].data(), d_out[lane].data(), bytes,
                               cudaMemcpyDeviceToHost, stream));
  }

  for (int k = std::max(0, chunks - kLanes); k < chunks; ++k) {
    collect(k);
  }
}

// --- tests -------------------------------------------------------------------

constexpr int kChunks = 4;
constexpr std::size_t kChunkSize = std::size_t{1} << 23; // 32 MB of floats
constexpr std::size_t kTotal = kChunkSize * kChunks;

std::vector<float> ramp(std::size_t n) {
  std::vector<float> v(n);
  std::iota(v.begin(), v.end(), 0.0F);
  return v;
}

TEST_CASE("every chunk is transformed") {
  const std::vector<float> input = ramp(kTotal);
  std::vector<float> output(kTotal, -1.0F);
  Trace trace(kChunks);

  process_chunks(input, output, kChunks, trace);
  CUDA_CHECK(cudaDeviceSynchronize());

  for (std::size_t i = 0; i < kTotal; ++i) {
    if (output[i] != input[i] * 2.0F + 1.0F) {
      CAPTURE(i);
      REQUIRE(output[i] == input[i] * 2.0F + 1.0F);
    }
  }
}

TEST_CASE("the upload of chunk 1 finishes while the kernel of chunk 0 is still running") {
  const std::vector<float> input = ramp(kTotal);
  std::vector<float> output(kTotal, -1.0F);
  Trace trace(kChunks);

  process_chunks(input, output, kChunks, trace);
  CUDA_CHECK(cudaDeviceSynchronize());

  // On one stream, chunk 1 cannot be uploaded until chunk 0 has been
  // downloaded, which is after its kernel finished: this number is negative.
  // With two streams and pinned staging it is several milliseconds positive,
  // because the kernel spins for ~8 ms and a 32 MB upload takes ~1.5 ms.
  const float headroom = elapsed_ms(trace.upload_done[1], trace.kernel_done[0]);
  MESSAGE("kernel 0 finished " << headroom << " ms after upload 1 finished");
  CHECK(headroom > 0.0F);
}

TEST_CASE("every event in the trace was recorded") {
  const std::vector<float> input = ramp(kTotal);
  std::vector<float> output(kTotal, -1.0F);
  Trace trace(kChunks);

  process_chunks(input, output, kChunks, trace);
  CUDA_CHECK(cudaDeviceSynchronize());

  for (int k = 0; k < kChunks; ++k) {
    CAPTURE(k);
    // cudaEventQuery reports cudaErrorNotReady for a recorded event whose
    // work is still pending, and cudaSuccess once it has completed or if it
    // was never recorded at all -- so pair it with a timing check.
    CHECK(cudaEventQuery(trace.upload_done[static_cast<std::size_t>(k)].get()) ==
          cudaSuccess);
    CHECK(elapsed_ms(trace.upload_done[static_cast<std::size_t>(k)],
                     trace.kernel_done[static_cast<std::size_t>(k)]) > 0.0F);
  }
}
