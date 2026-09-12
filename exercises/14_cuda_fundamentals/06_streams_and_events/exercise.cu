// =============================================================================
//  14.06 -- Streams and events
// =============================================================================
//
//  Everything you send to the device goes into a STREAM: an in-order queue of
//  work (Motta, ch. 5 "Parallelizing with streams"). Two operations in the
//  same stream run one after the other, in the order you enqueued them. Two
//  operations in DIFFERENT streams have no ordering at all -- the hardware
//  may run them concurrently, in either order, or overlapped, and it will
//  choose differently from run to run.
//
//  That is the whole point: a copy in one stream can overlap a kernel in
//  another, which 15.06 turns into a pipeline. It is also the whole danger.
//  A copy that reads a kernel's output from a different stream is a data
//  race unless you say otherwise, and the way to say so is an EVENT (Motta,
//  ch. 5 "Following the events"):
//
//      cudaEventRecord(done, compute);         // "here, in the compute stream"
//      cudaStreamWaitEvent(copy, done, 0);     // "copy stream: wait for that"
//      cudaMemcpyAsync(host, device, bytes, cudaMemcpyDeviceToHost, copy);
//
//  cudaEventRecord marks a point in a stream's queue; cudaStreamWaitEvent
//  makes everything enqueued into the other stream AFTER the wait hold until
//  that point has been reached. It is the device-side condition variable
//  (10.04), with the same rule: the wait goes before the work that depends
//  on it, or it protects nothing.
//
//  Three details that bite:
//
//   * When you name no stream you get the LEGACY DEFAULT STREAM, which
//     synchronises with every other blocking stream (Cautaerts & Ghorbanfekr,
//     ch. 6 "Implicit synchronization": kernel launches in the null stream
//     "have special synchronizing behavior with all other streams"). It is
//     safe and slow. Streams created with cudaStreamNonBlocking opt out of
//     that, and then you are responsible for every dependency yourself.
//
//   * cudaMemcpyAsync is only asynchronous from PINNED host memory
//     (cudaMallocHost). From ordinary pageable memory the runtime stages the
//     copy and returns when it is done, quietly turning your pipeline back
//     into a sequence. The DMA engine needs pages that cannot move.
//
//   * An event made with cudaEventDisableTiming is cheaper and is all you
//     need for ordering; one made without the flag can also be passed to
//     cudaEventElapsedTime, which is how 17.01 measures kernels from the
//     GPU's own clock.
//
//  The kernel in this exercise is deliberately slow -- tens of milliseconds
//  -- so that the race is not a one-in-a-hundred flake but a certainty: a
//  copy that does not wait will always read the output buffer before the
//  kernel has written it. Real races are rarely so obliging, which is why
//  chapter 17 also introduces `compute-sanitizer --tool racecheck`.
//
//  TASK
//    Make the copy stream wait for the kernel. Record an event after the
//    kernel in the compute stream, and make the copy stream wait on it before
//    the copy is enqueued.
//
//  RUN IT
//    ./mcpp test 14_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <chrono>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <cuda_runtime.h>

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
    CUDA_CHECK(cudaMemcpy(data_, host.data(), host.size_bytes(), cudaMemcpyHostToDevice));
  }

private:
  T* data_ = nullptr;
  std::size_t count_ = 0;
};

// Page-locked host memory. An asynchronous copy is only asynchronous when the
// host side cannot be paged out from under the DMA engine; from pageable
// memory the runtime falls back to a staged, synchronous copy. 15.06 measures
// the difference; here it is what makes the copy genuinely race the kernel.
template <typename T>
class PinnedBuffer {
public:
  explicit PinnedBuffer(std::size_t count) : count_(count) {
    CUDA_CHECK(cudaMallocHost(&data_, count_ * sizeof(T)));
  }
  ~PinnedBuffer() {
    cudaFreeHost(data_);
  }
  PinnedBuffer(const PinnedBuffer&) = delete;
  PinnedBuffer& operator=(const PinnedBuffer&) = delete;

  std::span<T> span() noexcept {
    return {data_, count_};
  }
  std::size_t bytes() const noexcept {
    return count_ * sizeof(T);
  }

private:
  T* data_ = nullptr;
  std::size_t count_ = 0;
};

// Two streams and the event that links them, created once and destroyed once.
// cudaEventDisableTiming makes an event cheaper when it is only ever used for
// ordering; an event created that way cannot be passed to
// cudaEventElapsedTime.
class Streams {
public:
  Streams() {
    CUDA_CHECK(cudaStreamCreateWithFlags(&compute_, cudaStreamNonBlocking));
    CUDA_CHECK(cudaStreamCreateWithFlags(&copy_, cudaStreamNonBlocking));
    CUDA_CHECK(cudaEventCreateWithFlags(&kernel_done_, cudaEventDisableTiming));
  }
  ~Streams() {
    cudaEventDestroy(kernel_done_);
    cudaStreamDestroy(copy_);
    cudaStreamDestroy(compute_);
  }
  Streams(const Streams&) = delete;
  Streams& operator=(const Streams&) = delete;

  cudaStream_t compute() const noexcept {
    return compute_;
  }
  cudaStream_t copy() const noexcept {
    return copy_;
  }
  cudaEvent_t kernel_done() const noexcept {
    return kernel_done_;
  }

private:
  cudaStream_t compute_{};
  cudaStream_t copy_{};
  cudaEvent_t kernel_done_{};
};

// Deliberately slow: a chain of dependent fused multiply-adds that converges
// to 1. Each iteration must wait for the previous one, so the kernel takes
// tens of milliseconds however many SMs the GPU has. That is what makes a
// missing dependency fail every time instead of once in a hundred runs.
constexpr int kSpin = 8'000'000;

__global__ void slow_add_one_kernel(const float* in, float* out, std::size_t n) {
  const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  if (i < n) {
    float x = 0.0F;
    for (int k = 0; k < kSpin; ++k) {
      x = fmaf(x, 0.999F, 0.001F);
    }
    out[i] = in[i] + x;
  }
}

constexpr float kSentinel = -1.0F;

// Enqueues: clear the output, run the slow kernel on the compute stream, then
// copy the result to `host` on the copy stream. Returns without waiting: the
// caller synchronises the copy stream (or queries it) when it wants the data.
void enqueue_pipeline(const Streams& streams, const DeviceBuffer<float>& d_in,
                      DeviceBuffer<float>& d_out, std::span<float> host) {
  const std::size_t n = d_in.size();
  constexpr unsigned kBlock = 256;
  const unsigned blocks = static_cast<unsigned>((n + kBlock - 1) / kBlock);

  // Same stream, so these two run in order: streams are in-order queues.
  CUDA_CHECK(cudaMemsetAsync(d_out.data(), 0xFF, d_out.bytes(), streams.compute()));
  slow_add_one_kernel<<<blocks, kBlock, 0, streams.compute()>>>(d_in.data(), d_out.data(),
                                                                n);
  CUDA_CHECK(cudaGetLastError());

  // TODO: the copy below is in a different stream from the kernel, so nothing
  // makes it wait. Record `streams.kernel_done()` in the compute stream here,
  // and make the copy stream wait on it before the copy is enqueued.

  CUDA_CHECK(cudaMemcpyAsync(host.data(), d_out.data(), d_out.bytes(),
                             cudaMemcpyDeviceToHost, streams.copy()));
}

namespace {

constexpr std::size_t kN = 1U << 16;

std::vector<float> inputs() {
  std::vector<float> in(kN);
  for (std::size_t i = 0; i < kN; ++i) {
    in[i] = static_cast<float>(i % 100);
  }
  return in;
}

} // namespace

TEST_CASE("the copy sees the kernel's output, not what was there before") {
  Streams streams;
  const std::vector<float> in = inputs();
  DeviceBuffer<float> d_in(kN);
  DeviceBuffer<float> d_out(kN);
  d_in.upload(in);
  PinnedBuffer<float> host(kN);
  for (float& value : host.span()) {
    value = kSentinel;
  }

  enqueue_pipeline(streams, d_in, d_out, host.span());
  CUDA_CHECK(cudaStreamSynchronize(streams.copy()));

  // The spin converges to exactly 1.0f, so the result is the input plus one.
  bool all = true;
  for (std::size_t i = 0; i < kN; ++i) {
    all = all && (host.span()[i] == doctest::Approx(in[i] + 1.0F).epsilon(1e-5));
  }
  CHECK(all);
  CHECK(host.span()[kN - 1] == doctest::Approx(in[kN - 1] + 1.0F));
}

TEST_CASE("the copy stream is still waiting while the kernel runs") {
  Streams streams;
  const std::vector<float> in = inputs();
  DeviceBuffer<float> d_in(kN);
  DeviceBuffer<float> d_out(kN);
  d_in.upload(in);
  PinnedBuffer<float> host(kN);

  enqueue_pipeline(streams, d_in, d_out, host.span());
  // A millisecond in: the kernel has at least ten more to go, and a copy that
  // depends on it must not have completed. cudaStreamQuery is the non-blocking
  // "are you done yet?"; cudaErrorNotReady is an answer, not a failure.
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  CHECK(cudaStreamQuery(streams.copy()) == cudaErrorNotReady);

  CUDA_CHECK(cudaStreamSynchronize(streams.copy()));
  CHECK(cudaStreamQuery(streams.copy()) == cudaSuccess);
  CHECK(host.span()[0] == doctest::Approx(1.0F));
}

TEST_CASE("events with timing measure the kernel from the GPU's clock") {
  Streams streams;
  const std::vector<float> in = inputs();
  DeviceBuffer<float> d_in(kN);
  DeviceBuffer<float> d_out(kN);
  d_in.upload(in);
  PinnedBuffer<float> host(kN);

  cudaEvent_t start = nullptr;
  cudaEvent_t stop = nullptr;
  CUDA_CHECK(cudaEventCreate(&start));
  CUDA_CHECK(cudaEventCreate(&stop));

  CUDA_CHECK(cudaEventRecord(start, streams.compute()));
  enqueue_pipeline(streams, d_in, d_out, host.span());
  CUDA_CHECK(cudaEventRecord(stop, streams.copy()));
  CUDA_CHECK(cudaEventSynchronize(stop));

  float ms = 0.0F;
  CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));
  CHECK(ms > 5.0F); // the spin alone is longer than this
  CHECK(ms < 5000.0F);

  CUDA_CHECK(cudaEventDestroy(stop));
  CUDA_CHECK(cudaEventDestroy(start));
}
