// =============================================================================
//  15.06 -- Pinned memory and copy/compute overlap
// =============================================================================
//
//  A GPU has three engines that can work at once: one that runs kernels, one
//  that copies host to device, and one that copies device to host (Motta
//  ch. 8, "Using CUDA streams to overlay operations"). A program that
//  uploads, computes, then downloads uses one of them at a time and leaves
//  the other two idle. Cut the data into chunks and put consecutive chunks
//  on different streams (14.06), and the upload of chunk k+1 can run while
//  the kernel of chunk k does, and the download of k-1 too. For a problem
//  where transfers take as long as compute, that is up to 3x.
//
//  It does not happen by itself, for a reason that lives in the operating
//  system. `cudaMemcpyAsync` from ordinary (PAGEABLE) host memory is not
//  asynchronous: the OS may move or swap a pageable buffer at any time, so
//  the GPU's DMA engine cannot read it directly. The driver copies it into an
//  internal PINNED buffer first, and that copy blocks the host thread
//  (Cautaerts ch. 6, "Asynchronous data transfers"; CUDA C++ Best Practices
//  Guide, "Pinned Memory"). Memory allocated with `cudaHostAlloc` (or
//  `cudaMallocHost`) is page-locked: the DMA engine reads it directly, the
//  call returns at once, and the transfer runs at full bus speed -- around
//  25 GB/s on the PCIe 4.0 x16 link an RTX 4090 sits on, against ~10 GB/s
//  through the staging path. Pinned memory is a scarce resource (the OS
//  cannot page it), so pin staging buffers, not whole data sets.
//
//  The pattern below is DOUBLE BUFFERING: two lanes, each a stream plus a
//  pinned staging pair and a device buffer pair. Chunk k uses lane k % 2,
//  and before a lane is reused the host waits for its stream, which by
//  stream order means chunk k-2 is entirely done. Meanwhile the host copies
//  the next chunk into the other lane's staging buffer -- a memcpy that
//  overlaps the kernel too.
//
//  The test does not time anything. It records an event after each upload
//  and each kernel and asks the GPU's clock whether upload 1 finished
//  BEFORE kernel 0 did. On a single stream that is impossible.
//
//  TASK
//    Rewrite `process_chunks` with pinned staging buffers, two streams and
//    two sets of device buffers. Keep recording the trace events.
//
//  RUN IT
//    ./mcpp test 15_06
//
// =============================================================================

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

// Processes `input` in `chunks` equal pieces.
//
// TODO: this is the serial version: one stream, one device buffer pair, and
// cudaMemcpyAsync straight from the caller's pageable vectors -- which makes
// each "async" copy a blocking one. Rewrite it with two lanes:
//   * a Stream, a PinnedBuffer<float> staging_in and staging_out, and a
//     DeviceBuffer<float> d_in and d_out per lane (kLanes = 2, chunk k uses
//     lane k % kLanes);
//   * before reusing a lane (k >= kLanes), cudaStreamSynchronize its stream
//     and copy its staging_out into `output` for chunk k - kLanes;
//   * std::copy_n chunk k of `input` into the lane's staging_in, then
//     cudaMemcpyAsync it up, record trace.upload_done[k], launch the kernel
//     on the lane's stream, record trace.kernel_done[k], cudaMemcpyAsync the
//     result down into staging_out;
//   * after the loop, synchronise each lane and copy out its last chunk.
void process_chunks(std::span<const float> input, std::span<float> output, int chunks,
                    Trace& trace) {
  const std::size_t chunk = input.size() / static_cast<std::size_t>(chunks);
  const std::size_t bytes = chunk * sizeof(float);

  Stream stream;
  DeviceBuffer<float> d_in(chunk);
  DeviceBuffer<float> d_out(chunk);

  for (int k = 0; k < chunks; ++k) {
    const std::size_t offset = static_cast<std::size_t>(k) * chunk;

    CUDA_CHECK(cudaMemcpyAsync(d_in.data(), input.data() + offset, bytes,
                               cudaMemcpyHostToDevice, stream.get()));
    trace.upload_done[static_cast<std::size_t>(k)].record(stream.get());

    slow_transform<<<kGrid, kBlock, 0, stream.get()>>>(d_in.data(), d_out.data(), chunk);
    CUDA_CHECK(cudaGetLastError());
    trace.kernel_done[static_cast<std::size_t>(k)].record(stream.get());

    CUDA_CHECK(cudaMemcpyAsync(output.data() + offset, d_out.data(), bytes,
                               cudaMemcpyDeviceToHost, stream.get()));
  }
  CUDA_CHECK(cudaStreamSynchronize(stream.get()));
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
