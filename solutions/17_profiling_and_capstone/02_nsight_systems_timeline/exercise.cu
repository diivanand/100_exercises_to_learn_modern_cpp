// Solution -- 17.02 The Nsight Systems timeline
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cuda_runtime.h>
#include <nvtx3/nvToolsExt.h>

// --- the helpers from 14.02 and 14.03 ----------------------------------------

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

// -----------------------------------------------------------------------------

constexpr unsigned kBlock = 256;

// One step of a toy iterative solver: y <- a * y + x. Thread 0 also records
// y[0] into `trace` for this step, so progress can be inspected later without
// a round trip to the host. It is the same thread that wrote y[0], so there is
// no race on it.
__global__ void step_kernel(const float* x, float* y, float a, std::size_t n,
                            float* trace, int step) {
  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    const float v = a * y[i] + x[i];
    y[i] = v;
    if (i == 0) {
      trace[step] = v;
    }
  }
}

// Runs `steps` steps and returns y[0] after each one.
//
// Nothing in the loop waits for the GPU. Launches are queued as fast as the
// host can issue them, the device drains the queue back to back, and the one
// synchronisation is the download at the end, which cudaMemcpy does
// implicitly. Errors from a launch are still caught: cudaGetLastError
// reports a bad configuration immediately, and an error inside a kernel is
// sticky, so the download's cudaMemcpy will report it.
std::vector<float> run_pipeline(const DeviceBuffer<float>& x, DeviceBuffer<float>& y,
                                float a, int steps) {
  nvtxRangePushA("run_pipeline");
  DeviceBuffer<float> trace(static_cast<std::size_t>(steps));
  const unsigned grid = static_cast<unsigned>(
      std::min<std::size_t>((x.size() + kBlock - 1) / kBlock, 1024));

  nvtxRangePushA("steps");
  for (int step = 0; step < steps; ++step) {
    step_kernel<<<grid, kBlock>>>(x.data(), y.data(), a, x.size(), trace.data(), step);
    CUDA_CHECK(cudaGetLastError());
  }
  nvtxRangePop();

  nvtxRangePushA("download trace");
  std::vector<float> host_trace(static_cast<std::size_t>(steps));
  trace.download(host_trace);
  nvtxRangePop();

  nvtxRangePop();
  return host_trace;
}

// -----------------------------------------------------------------------------

namespace {

constexpr std::size_t kN = std::size_t{1} << 16;
constexpr float kA = 0.999F;
constexpr int kSteps = 2000;

std::vector<float> make_x() {
  std::vector<float> x(kN);
  for (std::size_t i = 0; i < kN; ++i) {
    x[i] = 0.001F * static_cast<float>(i % 7);
  }
  return x;
}

// The same recurrence on the host, for one element.
std::vector<float> reference_trace(float x0, float y0, int steps) {
  std::vector<float> trace(static_cast<std::size_t>(steps));
  float y = y0;
  for (int step = 0; step < steps; ++step) {
    y = kA * y + x0;
    trace[static_cast<std::size_t>(step)] = y;
  }
  return trace;
}

// What an all-asynchronous pipeline costs on this machine, measured the same
// way as the function under test. The test compares against this rather than
// against a fixed number of milliseconds, so it holds on any GPU.
std::vector<float> reference_pipeline(const DeviceBuffer<float>& x,
                                      DeviceBuffer<float>& y, int steps) {
  DeviceBuffer<float> trace(static_cast<std::size_t>(steps));
  const unsigned grid = static_cast<unsigned>(
      std::min<std::size_t>((x.size() + kBlock - 1) / kBlock, 1024));
  for (int step = 0; step < steps; ++step) {
    step_kernel<<<grid, kBlock>>>(x.data(), y.data(), kA, x.size(), trace.data(), step);
    CUDA_CHECK(cudaGetLastError());
  }
  std::vector<float> host_trace(static_cast<std::size_t>(steps));
  trace.download(host_trace);
  return host_trace;
}

template <typename F>
double best_wall_ms(int repetitions, F&& work) {
  work(); // warm-up
  double best = 1e30;
  for (int i = 0; i < repetitions; ++i) {
    const auto begin = std::chrono::steady_clock::now();
    work();
    const auto end = std::chrono::steady_clock::now();
    best = std::min(best, std::chrono::duration<double, std::milli>(end - begin).count());
  }
  return best;
}

} // namespace

TEST_CASE("the trace matches a sequential reference") {
  const std::vector<float> host_x = make_x();
  const std::vector<float> host_y(kN, 1.0F);
  DeviceBuffer<float> x(kN);
  DeviceBuffer<float> y(kN);
  x.upload(host_x);
  y.upload(host_y);

  const std::vector<float> trace = run_pipeline(x, y, kA, kSteps);
  const std::vector<float> expected = reference_trace(host_x[0], 1.0F, kSteps);
  REQUIRE(trace.size() == expected.size());

  int mismatches = 0;
  for (std::size_t i = 0; i < trace.size(); ++i) {
    if (trace[i] != doctest::Approx(expected[i]).epsilon(1e-3)) {
      ++mismatches;
    }
  }
  CHECK(mismatches == 0);

  // The whole vector was updated, not just element 0.
  std::vector<float> back(kN);
  y.download(back);
  const std::vector<float> last = reference_trace(host_x[kN - 1], 1.0F, kSteps);
  CHECK(back[kN - 1] == doctest::Approx(last.back()).epsilon(1e-3));
}

TEST_CASE("a pipeline with no synchronisation inside runs at launch rate") {
  const std::vector<float> host_x = make_x();
  const std::vector<float> host_y(kN, 1.0F);
  DeviceBuffer<float> x(kN);
  DeviceBuffer<float> y(kN);
  x.upload(host_x);
  y.upload(host_y);

  const double reference_ms =
      best_wall_ms(3, [&] { (void)reference_pipeline(x, y, kSteps); });
  const double yours_ms = best_wall_ms(3, [&] { (void)run_pipeline(x, y, kA, kSteps); });
  MESSAGE("all-asynchronous: " << reference_ms << " ms, yours: " << yours_ms << " ms for "
                               << kSteps << " steps");

  // A synchronisation per step costs more than the step itself here, so the
  // synchronised version is several times slower. 2x leaves a wide margin.
  CHECK(yours_ms <= 2.0 * reference_ms);
}
