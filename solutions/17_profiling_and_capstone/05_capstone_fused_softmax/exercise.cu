// Solution -- 17.05 Capstone: a fused softmax
#include <doctest/doctest.h>

#include <algorithm>
#include <cfloat>
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

constexpr int kThreads = 256;
constexpr int kWarp = 32;
constexpr int kWarpsPerBlock = kThreads / kWarp;
static_assert(kWarpsPerBlock * kWarp == kThreads, "a block is a whole number of warps");

struct MaxOp {
  __device__ float operator()(float a, float b) const {
    return fmaxf(a, b);
  }
};
struct SumOp {
  __device__ float operator()(float a, float b) const {
    return a + b;
  }
};

// Reduces across the 32 lanes of a warp; lane 0 ends up with the result
// (15.03). No shared memory, no synchronisation: shuffles read registers.
template <typename Op>
__device__ float warp_reduce(float v, Op op) {
  for (int offset = kWarp / 2; offset > 0; offset /= 2) {
    v = op(v, __shfl_down_sync(0xFFFFFFFFU, v, offset));
  }
  return v;
}

// Reduces across the block and returns the result to EVERY thread. `scratch`
// holds one slot per warp. The final __syncthreads is not decoration: it lets
// the caller reuse `scratch` for the next reduction straight away.
template <typename Op>
__device__ float block_reduce(float v, float identity, Op op, float* scratch) {
  const int lane = threadIdx.x % kWarp;
  const int warp = threadIdx.x / kWarp;

  v = warp_reduce(v, op);
  if (lane == 0) {
    scratch[warp] = v;
  }
  __syncthreads();

  float r = (threadIdx.x < kWarpsPerBlock) ? scratch[threadIdx.x] : identity;
  if (warp == 0) {
    r = warp_reduce(r, op);
    if (lane == 0) {
      scratch[0] = r;
    }
  }
  __syncthreads();

  const float result = scratch[0];
  __syncthreads();
  return result;
}

// One block per row. Three passes over the row, all fused into one kernel:
//   1. the row maximum                       (stability: exp never overflows)
//   2. the sum of exp(x - max)
//   3. write exp(x - max) / sum
// The row is read three times, but the second and third reads hit L1/L2: a
// row of 1024 floats is 4 KB. What matters is that global memory is written
// once and there is a single launch, so nothing is stored and reloaded
// between passes. Consecutive threads read consecutive columns, so every
// access is coalesced (15.01).
__global__ void fused_softmax_kernel(const float* in, float* out, int cols) {
  __shared__ float scratch[kWarpsPerBlock];
  const std::size_t offset =
      static_cast<std::size_t>(blockIdx.x) * static_cast<std::size_t>(cols);
  const float* x = in + offset;
  float* y = out + offset;

  float local_max = -FLT_MAX;
  for (int c = static_cast<int>(threadIdx.x); c < cols; c += kThreads) {
    local_max = fmaxf(local_max, x[c]);
  }
  const float row_max = block_reduce(local_max, -FLT_MAX, MaxOp{}, scratch);

  float local_sum = 0.0F;
  for (int c = static_cast<int>(threadIdx.x); c < cols; c += kThreads) {
    local_sum += expf(x[c] - row_max);
  }
  const float row_sum = block_reduce(local_sum, 0.0F, SumOp{}, scratch);

  const float inverse = 1.0F / row_sum;
  for (int c = static_cast<int>(threadIdx.x); c < cols; c += kThreads) {
    y[c] = expf(x[c] - row_max) * inverse;
  }
}

// Row-wise softmax of a rows x cols matrix in row-major order.
void softmax_rows(const float* in, float* out, int rows, int cols) {
  fused_softmax_kernel<<<static_cast<unsigned>(rows), kThreads>>>(in, out, cols);
  CUDA_CHECK(cudaGetLastError());
}

// -----------------------------------------------------------------------------

namespace {

std::vector<float> make_logits(int rows, int cols, float low, float high) {
  std::vector<float> x(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols));
  // A deterministic pseudo-random sequence: reproducible and seed-free.
  unsigned state = 12345U;
  for (float& v : x) {
    state = state * 1664525U + 1013904223U;
    const float unit = static_cast<float>(state >> 8) / static_cast<float>(1U << 24);
    v = low + (high - low) * unit;
  }
  return x;
}

// Softmax in double precision, the stable way, on the host.
std::vector<float> reference_softmax(const std::vector<float>& x, int rows, int cols) {
  std::vector<float> y(x.size());
  for (int r = 0; r < rows; ++r) {
    const std::size_t base = static_cast<std::size_t>(r) * static_cast<std::size_t>(cols);
    double m = -1e300;
    for (int c = 0; c < cols; ++c) {
      m = std::max(m, static_cast<double>(x[base + static_cast<std::size_t>(c)]));
    }
    double s = 0.0;
    for (int c = 0; c < cols; ++c) {
      s += std::exp(static_cast<double>(x[base + static_cast<std::size_t>(c)]) - m);
    }
    for (int c = 0; c < cols; ++c) {
      const std::size_t i = base + static_cast<std::size_t>(c);
      y[i] = static_cast<float>(std::exp(static_cast<double>(x[i]) - m) / s);
    }
  }
  return y;
}

int count_mismatches(const std::vector<float>& got, const std::vector<float>& want,
                     float tolerance) {
  int mismatches = 0;
  for (std::size_t i = 0; i < got.size(); ++i) {
    if (!(std::fabs(got[i] - want[i]) <= tolerance)) {
      ++mismatches;
    }
  }
  return mismatches;
}

std::vector<float> run(const std::vector<float>& x, int rows, int cols) {
  DeviceBuffer<float> in(x.size());
  DeviceBuffer<float> out(x.size());
  in.upload(x);
  softmax_rows(in.data(), out.data(), rows, cols);
  CUDA_CHECK(cudaDeviceSynchronize());
  std::vector<float> y(x.size());
  out.download(y);
  return y;
}

// --- the unfused version, for the speed comparison ---------------------------
//
// What a first attempt tends to look like: one thread per element, one
// kernel per pass, the per-row statistics accumulated with atomics. It is
// correct and stable. It is also three launches, three full reads of the
// input, an extra write and read of the statistics, and a queue of threads
// fighting over one address per row.

__device__ float atomic_max_float(float* address, float value) {
  int* as_int = reinterpret_cast<int*>(address);
  int old = *as_int;
  int assumed = 0;
  do {
    assumed = old;
    const float candidate = fmaxf(value, __int_as_float(assumed));
    old = atomicCAS(as_int, assumed, __float_as_int(candidate));
  } while (assumed != old);
  return __int_as_float(old);
}

__global__ void unfused_row_max(const float* in, float* maxes, int cols) {
  const int row = static_cast<int>(blockIdx.x);
  for (int c = static_cast<int>(threadIdx.x); c < cols; c += kThreads) {
    atomic_max_float(&maxes[row], in[static_cast<std::size_t>(row) * cols + c]);
  }
}

__global__ void unfused_row_sum(const float* in, const float* maxes, float* sums,
                                int cols) {
  const int row = static_cast<int>(blockIdx.x);
  for (int c = static_cast<int>(threadIdx.x); c < cols; c += kThreads) {
    atomicAdd(&sums[row],
              expf(in[static_cast<std::size_t>(row) * cols + c] - maxes[row]));
  }
}

__global__ void unfused_normalise(const float* in, const float* maxes, const float* sums,
                                  float* out, int cols) {
  const int row = static_cast<int>(blockIdx.x);
  for (int c = static_cast<int>(threadIdx.x); c < cols; c += kThreads) {
    const std::size_t i = static_cast<std::size_t>(row) * cols + c;
    out[i] = expf(in[i] - maxes[row]) / sums[row];
  }
}

void unfused_softmax_rows(const float* in, float* out, int rows, int cols) {
  DeviceBuffer<float> maxes(static_cast<std::size_t>(rows));
  DeviceBuffer<float> sums(static_cast<std::size_t>(rows));
  std::vector<float> lowest(static_cast<std::size_t>(rows), -FLT_MAX);
  maxes.upload(lowest);
  CUDA_CHECK(cudaMemsetAsync(sums.data(), 0, sums.bytes()));
  const unsigned grid = static_cast<unsigned>(rows);
  unfused_row_max<<<grid, kThreads>>>(in, maxes.data(), cols);
  CUDA_CHECK(cudaGetLastError());
  unfused_row_sum<<<grid, kThreads>>>(in, maxes.data(), sums.data(), cols);
  CUDA_CHECK(cudaGetLastError());
  unfused_normalise<<<grid, kThreads>>>(in, maxes.data(), sums.data(), out, cols);
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());
}

} // namespace

TEST_CASE("softmax matches a double-precision reference") {
  constexpr int rows = 512;
  constexpr int cols = 1024;
  const std::vector<float> x = make_logits(rows, cols, -3.0F, 3.0F);
  const std::vector<float> got = run(x, rows, cols);
  const std::vector<float> want = reference_softmax(x, rows, cols);
  CHECK(count_mismatches(got, want, 1e-5F) == 0);
}

TEST_CASE("every row sums to one") {
  constexpr int rows = 64;
  constexpr int cols = 1024;
  const std::vector<float> x = make_logits(rows, cols, -3.0F, 3.0F);
  const std::vector<float> got = run(x, rows, cols);
  for (int r = 0; r < rows; ++r) {
    double s = 0.0;
    for (int c = 0; c < cols; ++c) {
      s += got[static_cast<std::size_t>(r) * cols + static_cast<std::size_t>(c)];
    }
    CHECK(s == doctest::Approx(1.0).epsilon(1e-4));
  }
}

TEST_CASE("large logits do not overflow") {
  // exp(200) is inf in single precision. Without subtracting the row maximum
  // the sum is inf and every element is inf / inf = NaN.
  constexpr int rows = 64;
  constexpr int cols = 1024;
  const std::vector<float> x = make_logits(rows, cols, 50.0F, 200.0F);
  const std::vector<float> got = run(x, rows, cols);
  const std::vector<float> want = reference_softmax(x, rows, cols);
  CHECK(count_mismatches(got, want, 1e-5F) == 0);
  CHECK(std::isfinite(got[0]));
}

TEST_CASE("the fused kernel beats the unfused version comfortably") {
  constexpr int rows = 4096;
  constexpr int cols = 1024;
  const std::vector<float> x = make_logits(rows, cols, -3.0F, 3.0F);
  DeviceBuffer<float> in(x.size());
  DeviceBuffer<float> out(x.size());
  in.upload(x);

  const float unfused_ms =
      best_of(5, [&] { unfused_softmax_rows(in.data(), out.data(), rows, cols); });
  const float fused_ms =
      best_of(5, [&] { softmax_rows(in.data(), out.data(), rows, cols); });
  MESSAGE("unfused: " << unfused_ms << " ms, yours: " << fused_ms << " ms for " << rows
                      << " x " << cols);

  // The unfused version is several times slower than a fused one; 1.5x is the
  // floor, not the target.
  CHECK(fused_ms * 1.5F <= unfused_ms);

  // And it still has to be right.
  CUDA_CHECK(cudaDeviceSynchronize());
  std::vector<float> got(x.size());
  out.download(got);
  CHECK(count_mismatches(got, reference_softmax(x, rows, cols), 1e-5F) == 0);
}
