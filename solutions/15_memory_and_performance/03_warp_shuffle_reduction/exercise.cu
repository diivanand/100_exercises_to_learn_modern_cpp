// Solution -- 15.03 Warp shuffle reduction
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <random>
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
  void start() {
    CUDA_CHECK(cudaEventRecord(start_, nullptr));
  }
  float stop() {
    CUDA_CHECK(cudaEventRecord(stop_, nullptr));
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
  GpuTimer timer;
  float best = 1e30F;
  work();
  CUDA_CHECK(cudaDeviceSynchronize());
  for (int i = 0; i < repetitions; ++i) {
    timer.start();
    work();
    best = std::min(best, timer.stop());
  }
  return best;
}

constexpr unsigned kWarp = 32;
constexpr unsigned kBlock = 256;
constexpr unsigned kMaxBlocks = 2048;
constexpr unsigned kFullMask = 0xffffffffU;

// Sums the 32 values of a warp into lane 0, in five steps and no memory at
// all: `__shfl_down_sync(mask, v, d)` hands each lane the value held by lane
// `lane + d`. After d = 16, lane i holds v[i] + v[i + 16]; after d = 8, four
// values; and so on down to d = 1. Lanes in the upper half receive their own
// value back, which is harmless: only lane 0's result is used.
__device__ unsigned long long warp_sum(unsigned long long value) {
  for (unsigned delta = kWarp / 2; delta > 0; delta /= 2) {
    value += __shfl_down_sync(kFullMask, value, delta);
  }
  return value;
}

// One partial sum per warp goes through shared memory; the first warp then
// reduces those. That is two rounds of shuffles and one barrier for a block of
// 256, against eight rounds of barriers for a tree in shared memory.
__global__ void reduce_sum_kernel(const int* data, std::size_t n,
                                  unsigned long long* result) {
  __shared__ unsigned long long partial[kBlock / kWarp];

  // Grid-stride loop (14.01): each thread folds several elements into a
  // register before any cross-thread work starts. Most of the "reduction" is
  // this loop; the shuffles only tidy up 256 numbers per block.
  unsigned long long value = 0;
  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    value += static_cast<unsigned long long>(data[i]);
  }

  value = warp_sum(value);

  const unsigned lane = threadIdx.x % kWarp;
  const unsigned warp = threadIdx.x / kWarp;
  if (lane == 0) {
    partial[warp] = value;
  }
  __syncthreads();

  if (warp == 0) {
    value = (lane < blockDim.x / kWarp) ? partial[lane] : 0;
    value = warp_sum(value);
    if (lane == 0) {
      // One atomic per block. With a few thousand blocks that is nothing;
      // one atomic per ELEMENT would serialise the whole problem (15.07).
      atomicAdd(result, value);
    }
  }
}

unsigned long long reduce_sum(const int* data, std::size_t n) {
  DeviceBuffer<unsigned long long> d_result(1);
  CUDA_CHECK(cudaMemset(d_result.data(), 0, sizeof(unsigned long long)));
  if (n > 0) {
    const auto blocks = static_cast<unsigned>(
        std::min<std::size_t>((n + kBlock - 1) / kBlock, kMaxBlocks));
    reduce_sum_kernel<<<blocks, kBlock>>>(data, n, d_result.data());
    CUDA_CHECK(cudaGetLastError());
  }
  unsigned long long result = 0;
  d_result.download(std::span<unsigned long long>{&result, 1});
  return result;
}

// --- tests -------------------------------------------------------------------

// The reduction from PMPP Figure 6.2: one element per thread, a tree in
// shared memory with a barrier per level, and the `% (2 * stride)` test that
// leaves every warp partly idle from the first level on. Correct, and slow
// for three separate reasons; 15.03 fixes all three.
__global__ void reference_reduce_kernel(const int* data, std::size_t n,
                                        unsigned long long* result) {
  __shared__ unsigned long long partial[kBlock];
  const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  partial[threadIdx.x] = i < n ? static_cast<unsigned long long>(data[i]) : 0;
  __syncthreads();
  for (unsigned stride = 1; stride < blockDim.x; stride *= 2) {
    if (threadIdx.x % (2 * stride) == 0) {
      partial[threadIdx.x] += partial[threadIdx.x + stride];
    }
    __syncthreads();
  }
  if (threadIdx.x == 0) {
    atomicAdd(result, partial[0]);
  }
}

unsigned long long reference_reduce(const int* data, std::size_t n) {
  DeviceBuffer<unsigned long long> d_result(1);
  CUDA_CHECK(cudaMemset(d_result.data(), 0, sizeof(unsigned long long)));
  if (n > 0) {
    const auto blocks = static_cast<unsigned>((n + kBlock - 1) / kBlock);
    reference_reduce_kernel<<<blocks, kBlock>>>(data, n, d_result.data());
    CUDA_CHECK(cudaGetLastError());
  }
  unsigned long long result = 0;
  d_result.download(std::span<unsigned long long>{&result, 1});
  return result;
}

std::vector<int> random_ints(std::size_t n, int high) {
  std::mt19937 engine{42};
  std::uniform_int_distribution<int> dist{0, high};
  std::vector<int> v(n);
  for (int& x : v) {
    x = dist(engine);
  }
  return v;
}

unsigned long long exact_sum(std::span<const int> values) {
  return std::accumulate(values.begin(), values.end(), 0ULL);
}

TEST_CASE("sums exactly, beyond what a float can hold") {
  // 16M values up to 1000: the total is around 8.4e9, which float represents
  // to within +-512. An accumulator that rounds is caught here.
  const std::size_t n = std::size_t{1} << 24;
  const std::vector<int> values = random_ints(n, 1000);
  DeviceBuffer<int> d_values(n);
  d_values.upload(values);
  CHECK(reduce_sum(d_values.data(), n) == exact_sum(values));
}

TEST_CASE("sizes that are not a whole number of blocks, or of warps") {
  for (const std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{31},
                              std::size_t{1000}, std::size_t{12345}}) {
    const std::vector<int> values = random_ints(n, 100);
    DeviceBuffer<int> d_values(n);
    d_values.upload(values);
    CAPTURE(n);
    CHECK(reduce_sum(d_values.data(), n) == exact_sum(values));
  }
}

TEST_CASE("the reference reduction agrees") {
  const std::size_t n = 100000;
  const std::vector<int> values = random_ints(n, 1000);
  DeviceBuffer<int> d_values(n);
  d_values.upload(values);
  CHECK(reference_reduce(d_values.data(), n) == exact_sum(values));
}

TEST_CASE("shuffles and a grid-stride loop beat a shared-memory tree") {
  const std::size_t n = std::size_t{1} << 24;
  const std::vector<int> values = random_ints(n, 1000);
  DeviceBuffer<int> d_values(n);
  d_values.upload(values);

  const float reference = best_of(5, [&] { reference_reduce(d_values.data(), n); });
  const float yours = best_of(5, [&] { reduce_sum(d_values.data(), n); });

  const double bytes = static_cast<double>(n) * sizeof(int);
  MESSAGE("reference: " << reference << " ms (" << bytes / reference / 1e6
                        << " GB/s), yours: " << yours << " ms (" << bytes / yours / 1e6
                        << " GB/s)");
  // Around 2-3x on an RTX 4090; the bound is half of that.
  CHECK(yours * 1.3F < reference);
}
