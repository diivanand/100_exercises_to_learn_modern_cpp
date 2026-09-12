// =============================================================================
//  15.04 -- Occupancy and launch bounds
// =============================================================================
//
//  A GPU hides latency with parallelism, not with caches. When a warp issues a
//  load that takes 500 cycles, the SM does not wait: it switches to another
//  resident warp that has work ready (PMPP §4.5, "Thread Scheduling and
//  Latency Tolerance"). That only works if there ARE other resident warps.
//  OCCUPANCY is the fraction of an SM's warp slots that are filled
//  (Cautaerts ch. 5, "Maximizing occupancy"), and it is bounded by whichever
//  of the SM's resources runs out first:
//
//      threads per SM      1536 on Ada (PMPP's G80: 768)
//      blocks per SM       24 (G80: 8)
//      registers per SM    65536 32-bit registers (G80: 8192)
//      shared memory       up to 100 KB of the 128 KB L1/shared (G80: 16 KB)
//
//  PMPP §6.3 ("Dynamic Partitioning of SM Resources") shows the trap in the
//  register budget: a kernel that uses one more register per thread can drop
//  from three resident blocks to two and lose a third of its warps -- a
//  "performance cliff" that no line of source code names. Block size has the
//  same shape of problem from the other side: 32-thread blocks hit the
//  24-blocks-per-SM ceiling at 768 threads, half the SM.
//
//  The tools for this are three runtime calls and one attribute:
//
//    cudaOccupancyMaxActiveBlocksPerMultiprocessor(&blocks, kernel, block, 0)
//        how many blocks of this size fit on one SM, given the kernel's
//        actual register and shared memory use. Occupancy is then
//        blocks * block / maxThreadsPerMultiProcessor.
//    cudaOccupancyMaxPotentialBlockSize(&minGrid, &block, kernel, 0, limit)
//        the block size that maximises occupancy, and the smallest grid that
//        puts one such block on every SM. The right way to size a grid-stride
//        kernel (14.01): enough blocks to fill the device, not one per 256
//        elements.
//    __launch_bounds__(maxThreads, minBlocks)
//        on the kernel itself: a promise that it is never launched with more
//        than maxThreads, and a request for at least minBlocks resident per
//        SM. The compiler then caps its register use to make that fit. The
//        cap is visible through cudaFuncGetAttributes as maxThreadsPerBlock.
//
//  Occupancy is not speed. A kernel at 50% occupancy that streams memory
//  perfectly beats one at 100% that does not (15.01), and past roughly 50%
//  the returns fall off quickly. It is the thing to check when a kernel is
//  slow and the profiler (17.03) says the warps are stalled waiting.
//
//  TASK
//    `choose_launch` hard-codes 32-thread blocks and one block per 32
//    elements. Ask the occupancy API instead. Implement
//    `theoretical_occupancy`, and give `axpy` launch bounds.
//
//  RUN IT
//    ./mcpp test 15_04
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

// The block size this kernel is designed for, and the smallest number of
// blocks per SM we want resident. Both go into __launch_bounds__.
constexpr int kMaxThreadsPerBlock = 256;
constexpr int kMinBlocksPerSm = 4;

// TODO: declare `__launch_bounds__(kMaxThreadsPerBlock, kMinBlocksPerSm)`
// between `__global__ void` and the kernel name. Without it the compiler
// assumes blocks of up to 1024 threads, and is free to use as many registers
// as it likes.
__global__ void axpy(float a, const float* x, float* y, std::size_t n) {
  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    y[i] = a * x[i] + y[i];
  }
}

int device_attribute(cudaDeviceAttr attribute) {
  int device = 0;
  CUDA_CHECK(cudaGetDevice(&device));
  int value = 0;
  CUDA_CHECK(cudaDeviceGetAttribute(&value, attribute, device));
  return value;
}

// Theoretical occupancy of `axpy` at a given block size: the fraction of the
// SM's thread slots that would be filled if blocks of that size were resident
// up to every limit -- threads per SM, blocks per SM, registers, shared memory.
//
// TODO: ask cudaOccupancyMaxActiveBlocksPerMultiprocessor how many blocks of
// `block_size` threads fit on one SM, and divide the threads that gives by
// cudaDevAttrMaxThreadsPerMultiProcessor.
double theoretical_occupancy([[maybe_unused]] int block_size) {
  return 1.0; // and drop the [[maybe_unused]] once block_size is used
}

struct LaunchConfig {
  unsigned grid;
  unsigned block;
};

// TODO: 32-thread blocks cap the SM at half its threads, and one block per
// 32 elements launches a million blocks for a kernel that already loops.
// Ask cudaOccupancyMaxPotentialBlockSize(&min_grid, &block, axpy, 0,
// kMaxThreadsPerBlock) for the block size, then launch the smaller of
// `min_grid` and the number of blocks needed to give every element a
// thread -- but never fewer than one.
LaunchConfig choose_launch(std::size_t n) {
  const unsigned block = 32;
  const std::size_t grid = (n + block - 1) / block;
  return {static_cast<unsigned>(grid), block};
}

void launch_axpy(float a, const float* x, float* y, std::size_t n) {
  const LaunchConfig config = choose_launch(n);
  axpy<<<config.grid, config.block>>>(a, x, y, n);
  CUDA_CHECK(cudaGetLastError());
}

// --- tests -------------------------------------------------------------------

TEST_CASE("the chosen block size is warp-aligned and fills the SM") {
  const LaunchConfig config = choose_launch(std::size_t{1} << 20);
  const int warp = device_attribute(cudaDevAttrWarpSize);
  MESSAGE("block " << config.block << ", grid " << config.grid << ", occupancy "
                   << theoretical_occupancy(static_cast<int>(config.block)));
  CHECK(config.block % static_cast<unsigned>(warp) == 0);
  CHECK(config.block >= 128);
  CHECK(config.block <= static_cast<unsigned>(kMaxThreadsPerBlock));
  CHECK(theoretical_occupancy(static_cast<int>(config.block)) >= 0.75);
}

TEST_CASE("the grid is large enough to fill the device, and no larger than needed") {
  const int sms = device_attribute(cudaDevAttrMultiProcessorCount);
  const LaunchConfig big = choose_launch(std::size_t{1} << 26);
  CHECK(big.grid >= static_cast<unsigned>(sms));

  const LaunchConfig tiny = choose_launch(100);
  CHECK(tiny.grid == 1);
}

TEST_CASE("occupancy depends on the block size in the way the header describes") {
  // 32-thread blocks run into the blocks-per-SM ceiling long before the
  // threads-per-SM one (24 blocks x 32 = 768 of 1536 on Ada); 256-thread
  // blocks fill the SM exactly.
  CHECK(theoretical_occupancy(32) < theoretical_occupancy(256));
  CHECK(theoretical_occupancy(256) > 0.9);
  CHECK(theoretical_occupancy(128) <= 1.0);
}

TEST_CASE("the kernel declares its launch bounds") {
  cudaFuncAttributes attributes{};
  CUDA_CHECK(cudaFuncGetAttributes(&attributes, axpy));
  MESSAGE("registers per thread: " << attributes.numRegs);
  CHECK(attributes.maxThreadsPerBlock == kMaxThreadsPerBlock);
  // 65536 registers per SM shared by kMinBlocksPerSm blocks of
  // kMaxThreadsPerBlock threads.
  CHECK(attributes.numRegs <= 65536 / (kMaxThreadsPerBlock * kMinBlocksPerSm));
}

TEST_CASE("axpy is still right") {
  const std::size_t n = 1000003;
  std::vector<float> x(n);
  std::vector<float> y(n);
  std::iota(x.begin(), x.end(), 0.0F);
  std::fill(y.begin(), y.end(), 1.0F);

  DeviceBuffer<float> d_x(n);
  DeviceBuffer<float> d_y(n);
  d_x.upload(x);
  d_y.upload(y);
  launch_axpy(2.0F, d_x.data(), d_y.data(), n);
  CUDA_CHECK(cudaDeviceSynchronize());
  d_y.download(y);

  for (std::size_t i = 0; i < n; ++i) {
    if (y[i] != 2.0F * x[i] + 1.0F) {
      CAPTURE(i);
      REQUIRE(y[i] == 2.0F * x[i] + 1.0F);
    }
  }
}
