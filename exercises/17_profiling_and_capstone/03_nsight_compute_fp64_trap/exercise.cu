// =============================================================================
//  17.03 -- Nsight Compute and the FP64 trap
// =============================================================================
//
//  Nsight Systems tells you WHICH kernel is slow. Nsight Compute tells you
//  WHY: it replays one kernel many times, collecting hardware counters each
//  time, and reports them by section (Cautaerts ch. 4, "Profiling sections
//  with Nsight Compute"; Motta ch. 7 for the GUI).
//
//  RUN IT on this starter:
//
//      ./mcpp profile 17_03 --ncu
//
//  which runs `ncu --set full --import-source yes` and writes a report you
//  open with `ncu-ui`. If it fails with ERR_NVGPUCTRPERM, the driver is
//  refusing to expose the performance counters to non-root users. Motta's
//  fix (ch. 7, "Configuring access to GPU performance counters"): create
//  /etc/modprobe.d/nvidia.conf containing
//      options nvidia NVreg_RestrictProfilingToAdminUsers=0
//  and reboot. Then, in the report for `evaluate_kernel`, read:
//
//   * GPU Speed Of Light Throughput -- two bars, Compute (SM) and Memory.
//     A copy kernel (17.01) pins the memory bar. This kernel should be
//     compute-bound: 256 multiply-adds per 8 bytes moved is far above the
//     ridge point of the roofline (Cautaerts ch. 4, "Memory- versus
//     compute-bound kernels"). If BOTH bars are low, something is
//     throttling the SMs from inside.
//   * Compute Workload Analysis, "Pipe Utilization" -- which execution
//     pipe is busy. In the starter it is the FP64 pipe, at 100%, while
//     FMA sits idle. That is the whole diagnosis.
//   * Source Counters / Source page -- because the build passes -lineinfo
//     (cmake/Cuda.cmake), the report attributes instructions and stalls to
//     lines of THIS file. Sort by "Warp Stall Sampling" and it points at
//     the Horner loop. Look at the SASS column: DFMA, DMUL, and a call to
//     the double-precision pow.
//
//  WHY IT MATTERS. GeForce parts run double precision at 1/64 of the
//  single-precision rate (an RTX 4090: ~83 TFLOPS FP32, ~1.3 TFLOPS FP64).
//  Nothing in the language warns you: `y * x + 1.0` promotes to double
//  because `1.0` is a double, and `pow(y, 2.0)` is the double-precision pow.
//  PMPP ch. 7 explains what double buys you (precision, range); this
//  exercise shows what it costs when you did not ask for it. If you need
//  double, use it deliberately. If you do not, spell your literals `1.0F`,
//  square with `y * y`, and reach for the float math functions (`expf`,
//  `sqrtf`) or, where accuracy allows, the intrinsics (`__expf`; Cautaerts
//  ch. 5, "Using intrinsic and libdevice functions"). Motta ch. 7 ("Using
//  fused instructions") shows the other half: `y * x + c` in float compiles
//  to one FMA, rounded once.
//
//  TASK
//    Make `evaluate_kernel` single precision throughout, without changing
//    the maths.
//
//  RUN IT
//    ./mcpp test 17_03
//
// =============================================================================
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cuda_runtime.h>

// --- the helpers from 14.02, 14.03 and 17.01 ---------------------------------

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

class GpuTimer {
public:
  GpuTimer() {
    CUDA_CHECK(cudaEventCreate(&start_));
    CUDA_CHECK(cudaEventCreate(&stop_));
  }
  ~GpuTimer() {
    cudaEventDestroy(start_);
    cudaEventDestroy(stop_);
  }
  GpuTimer(const GpuTimer&) = delete;
  GpuTimer& operator=(const GpuTimer&) = delete;

  void start(cudaStream_t stream = nullptr) {
    CUDA_CHECK(cudaEventRecord(start_, stream));
  }
  float stop(cudaStream_t stream = nullptr) {
    CUDA_CHECK(cudaEventRecord(stop_, stream));
    CUDA_CHECK(cudaEventSynchronize(stop_));
    float ms = 0.0F;
    CUDA_CHECK(cudaEventElapsedTime(&ms, start_, stop_));
    return ms;
  }

private:
  cudaEvent_t start_{};
  cudaEvent_t stop_{};
};

template <typename F>
float best_of(int repetitions, F&& work) {
  work();
  CUDA_CHECK(cudaDeviceSynchronize());
  GpuTimer timer;
  float best = 1e30F;
  for (int i = 0; i < repetitions; ++i) {
    timer.start();
    work();
    best = std::min(best, timer.stop());
  }
  return best;
}

// -----------------------------------------------------------------------------

constexpr unsigned kBlock = 256;
constexpr int kTerms = 256;

// Evaluates p(x) = sum_{k} x^k / (k + 1) by Horner's rule, and squares it.
__global__ void evaluate_kernel(const float* xs, float* out, std::size_t n) {
  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    const float x = xs[i];
    // TODO: every operation in this loop is double precision. `y` is a
    // double, `1.0` is a double literal, and `pow(y, 2.0)` is the
    // double-precision pow. The input and output are float; nothing here
    // needed the extra precision, and the profiler shows the FP64 pipe
    // saturated while the FMA pipe idles.
    double y = 0.0;
    for (int k = 0; k < kTerms; ++k) {
      y = y * x + 1.0 / (k + 1);
    }
    out[i] = pow(y, 2.0);
  }
}

void evaluate(const DeviceBuffer<float>& xs, DeviceBuffer<float>& out) {
  const unsigned grid = static_cast<unsigned>(
      std::min<std::size_t>((xs.size() + kBlock - 1) / kBlock, 4096));
  evaluate_kernel<<<grid, kBlock>>>(xs.data(), out.data(), xs.size());
  CUDA_CHECK(cudaGetLastError());
}

// -----------------------------------------------------------------------------

namespace {

constexpr std::size_t kN = std::size_t{1} << 22;

std::vector<float> make_inputs() {
  std::vector<float> xs(kN);
  for (std::size_t i = 0; i < kN; ++i) {
    // Spread over [-0.9, 0.9] so the series converges comfortably.
    xs[i] = -0.9F + 1.8F * static_cast<float>(i % 1000) / 999.0F;
  }
  return xs;
}

// The same arithmetic on the host, in float, in the same order.
float reference_evaluate(float x) {
  float y = 0.0F;
  for (int k = 0; k < kTerms; ++k) {
    y = y * x + 1.0F / static_cast<float>(k + 1);
  }
  return y * y;
}

// A single-precision version of the kernel, compiled into the test, so the
// speed bound is relative to this GPU rather than a fixed number.
__global__ void reference_kernel(const float* xs, float* out, std::size_t n) {
  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    const float x = xs[i];
    float y = 0.0F;
    for (int k = 0; k < kTerms; ++k) {
      y = y * x + 1.0F / static_cast<float>(k + 1);
    }
    out[i] = y * y;
  }
}

} // namespace

TEST_CASE("the polynomial is evaluated correctly") {
  const std::vector<float> host_xs = make_inputs();
  DeviceBuffer<float> xs(kN);
  DeviceBuffer<float> out(kN);
  xs.upload(host_xs);

  evaluate(xs, out);
  CUDA_CHECK(cudaDeviceSynchronize());
  std::vector<float> result(kN);
  out.download(result);

  int mismatches = 0;
  for (std::size_t i = 0; i < kN; ++i) {
    const float expected = reference_evaluate(host_xs[i]);
    // Fused multiply-add on the device rounds once per step where the host
    // rounds twice, so the answers differ in the last bits.
    if (std::fabs(result[i] - expected) > 1e-4F * std::fabs(expected) + 1e-6F) {
      ++mismatches;
    }
  }
  CHECK(mismatches == 0);
}

TEST_CASE("the kernel runs at single-precision speed") {
  const std::vector<float> host_xs = make_inputs();
  DeviceBuffer<float> xs(kN);
  DeviceBuffer<float> out(kN);
  xs.upload(host_xs);
  const unsigned grid =
      static_cast<unsigned>(std::min<std::size_t>((kN + kBlock - 1) / kBlock, 4096));

  const float reference_ms = best_of(5, [&] {
    reference_kernel<<<grid, kBlock>>>(xs.data(), out.data(), kN);
    CUDA_CHECK(cudaGetLastError());
  });
  const float yours_ms = best_of(5, [&] { evaluate(xs, out); });
  MESSAGE("single precision: " << reference_ms << " ms, yours: " << yours_ms << " ms");

  // On a GeForce part double precision runs at 1/64 of single, so a kernel
  // that has quietly promoted to double is tens of times slower, not 2.5x.
  CHECK(yours_ms <= 2.5F * reference_ms);
}
