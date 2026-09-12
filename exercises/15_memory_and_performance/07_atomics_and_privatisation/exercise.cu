// =============================================================================
//  15.07 -- Atomics and privatisation
// =============================================================================
//
//  A histogram is the simplest computation in which many threads must update
//  the same few locations: 64 million bytes, 256 counters. Two threads doing
//  `bins[x]++` on the same bin at the same time lose an increment -- the
//  read-modify-write is three steps and they interleave (10.02, the same
//  data race on a CPU). `atomicAdd(&bins[x], 1)` makes the three steps one
//  (Cautaerts ch. 3, "Using atomics"; PMPP §12.2.4). It is correct. It is
//  also the slowest correct program you can write for this problem.
//
//  An atomic on global memory is performed at the L2 cache, and atomics to
//  the SAME address serialise there: however many threads want to increment
//  bin 'e', the increments happen one at a time. With text-like input most
//  bytes fall into a handful of bins, so most of 64 million atomics queue on
//  a handful of addresses. PMPP wrote in 2010 that Fermi's faster atomics
//  "reduce the need for complex algorithm transformations such as prefix
//  scanning and sorting" for histograms; Ada's L2 atomics are faster still,
//  and the queue on a hot address is still a queue.
//
//  PRIVATISATION is the standard fix (the second edition of PMPP gives it a
//  chapter). Give each block a private copy of the histogram in shared
//  memory. Shared-memory atomics are on-chip -- native since Maxwell, where
//  Fermi and Kepler emulated them with a lock loop -- and contend only among
//  the block's 256 threads. When the block has consumed its share of the
//  input, it adds its 256 private counts to the global histogram with 256
//  global atomics: one per bin per block, rather than one per element. For
//  64 MB in 2048 blocks that is 500,000 global atomics instead of 67 million,
//  and the ones that remain are spread over time.
//
//  Three details make the kernel:
//   * the private copy must be zeroed before use and `__syncthreads()`'d
//     after zeroing, and again after counting, before the flush;
//   * a grid-stride loop (14.01) with a bounded grid, so that each block does
//     enough work to amortise its 256-atomic flush;
//   * flush every bin, empty or not -- a branch on the count would make the
//     warp diverge (15.03) to save an atomic that costs less than the branch.
//
//  TASK
//    `histogram_kernel` does one global atomic per element. Privatise it.
//
//  RUN IT
//    ./mcpp test 15_07
//
// =============================================================================

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

// TODO: one global atomic per element, all of them queueing on the same 256
// addresses. Count into a `__shared__ unsigned local[kBins]` instead: zero it
// (one bin per thread), __syncthreads(), count with atomicAdd on `local`,
// __syncthreads(), then atomicAdd each thread's bin of `local` into `bins`.
__global__ void histogram_kernel(const std::uint8_t* data, std::size_t n,
                                 unsigned* bins) {
  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    atomicAdd(&bins[data[i]], 1U);
  }
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
