// Solution -- 15.07 Atomics and privatisation
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cuda_runtime.h>

// From 14.02, 14.03 and 15.01.
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

constexpr unsigned kBins = 256;
constexpr unsigned kBlock = 256;
constexpr unsigned kMaxBlocks = 2048;
static_assert(kBlock == kBins, "one thread per bin makes the flush a single statement");

using Histogram = std::array<unsigned, kBins>;

// Each block counts into its own copy of the histogram in shared memory and
// adds the copy to the global one once. The shared-memory atomics are
// on-chip and contend only among 256 threads; the global atomics are reduced
// from one per element to 256 per block.
__global__ void histogram_kernel(const std::uint8_t* data, std::size_t n,
                                 unsigned* bins) {
  __shared__ unsigned local[kBins];
  local[threadIdx.x] = 0;
  __syncthreads();

  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    atomicAdd(&local[data[i]], 1U);
  }
  __syncthreads();

  // Every bin is flushed, including empty ones: a branch on `local[...] != 0`
  // would save a few atomics and cost a divergent warp.
  atomicAdd(&bins[threadIdx.x], local[threadIdx.x]);
}

void launch_histogram(const std::uint8_t* data, std::size_t n, unsigned* bins) {
  CUDA_CHECK(cudaMemset(bins, 0, kBins * sizeof(unsigned)));
  if (n == 0) {
    return;
  }
  const auto blocks =
      static_cast<unsigned>(std::min<std::size_t>((n + kBlock - 1) / kBlock, kMaxBlocks));
  histogram_kernel<<<blocks, kBlock>>>(data, n, bins);
  CUDA_CHECK(cudaGetLastError());
}

Histogram histogram_of(const DeviceBuffer<std::uint8_t>& data) {
  DeviceBuffer<unsigned> d_bins(kBins);
  launch_histogram(data.data(), data.size(), d_bins.data());
  Histogram bins{};
  d_bins.download(bins);
  return bins;
}

// --- tests -------------------------------------------------------------------

// One global atomic per element. Correct, and every one of the 64M
// increments below queues up on the same 256 addresses in L2.
__global__ void reference_histogram_kernel(const std::uint8_t* data, std::size_t n,
                                           unsigned* bins) {
  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    atomicAdd(&bins[data[i]], 1U);
  }
}

void launch_reference(const std::uint8_t* data, std::size_t n, unsigned* bins) {
  CUDA_CHECK(cudaMemset(bins, 0, kBins * sizeof(unsigned)));
  if (n == 0) {
    return;
  }
  const auto blocks =
      static_cast<unsigned>(std::min<std::size_t>((n + kBlock - 1) / kBlock, kMaxBlocks));
  reference_histogram_kernel<<<blocks, kBlock>>>(data, n, bins);
  CUDA_CHECK(cudaGetLastError());
}

// Text-like data: most bytes fall into a few bins. Skew is what makes atomics
// contend -- a uniform stream would spread the load over all 256 addresses.
std::vector<std::uint8_t> skewed_bytes(std::size_t n) {
  std::vector<std::uint8_t> v(n);
  std::uint32_t state = 12345;
  for (std::uint8_t& x : v) {
    state = state * 1664525U + 1013904223U; // a linear congruential generator
    const std::uint32_t r = state >> 8;
    x = static_cast<std::uint8_t>((r % 100 < 80) ? 'a' + (r / 100) % 16
                                                 : (r / 100) % 256);
  }
  return v;
}

Histogram host_histogram(std::span<const std::uint8_t> data) {
  Histogram bins{};
  for (const std::uint8_t x : data) {
    ++bins[x];
  }
  return bins;
}

TEST_CASE("counts every byte exactly once") {
  const std::size_t n = std::size_t{1} << 26; // 64 MB
  const std::vector<std::uint8_t> data = skewed_bytes(n);
  DeviceBuffer<std::uint8_t> d_data(n);
  d_data.upload(data);

  const Histogram bins = histogram_of(d_data);
  const Histogram expected = host_histogram(data);
  for (unsigned b = 0; b < kBins; ++b) {
    if (bins[b] != expected[b]) {
      CAPTURE(b);
      REQUIRE(bins[b] == expected[b]);
    }
  }
}

TEST_CASE("empty, tiny and ragged inputs") {
  for (const std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{255},
                              std::size_t{257}, std::size_t{100003}}) {
    const std::vector<std::uint8_t> data = skewed_bytes(n);
    DeviceBuffer<std::uint8_t> d_data(n);
    d_data.upload(data);
    CAPTURE(n);
    CHECK(histogram_of(d_data) == host_histogram(data));
  }
}

TEST_CASE("privatisation beats one global atomic per element") {
  const std::size_t n = std::size_t{1} << 26;
  const std::vector<std::uint8_t> data = skewed_bytes(n);
  DeviceBuffer<std::uint8_t> d_data(n);
  d_data.upload(data);
  DeviceBuffer<unsigned> d_bins(kBins);

  const float reference =
      best_of(5, [&] { launch_reference(d_data.data(), n, d_bins.data()); });
  const float yours =
      best_of(5, [&] { launch_histogram(d_data.data(), n, d_bins.data()); });
  CUDA_CHECK(cudaDeviceSynchronize());

  MESSAGE("global atomics: " << reference << " ms, privatised: " << yours << " ms");
  // Several times faster on an RTX 4090; half of the smallest plausible gain
  // is required.
  CHECK(yours * 1.5F < reference);
}
