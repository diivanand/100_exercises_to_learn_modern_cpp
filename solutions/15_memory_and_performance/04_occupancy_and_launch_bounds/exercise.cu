// Solution -- 15.04 Occupancy and launch bounds
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

// `__launch_bounds__(maxThreads, minBlocks)` is a promise to the compiler:
// this kernel is never launched with more than `maxThreads` per block, and we
// want at least `minBlocks` of them resident per SM. The compiler then limits
// register use to what makes that fit (65536 registers per SM / (256 x 4) =
// 64 per thread here), spilling to local memory if it must, instead of
// grabbing as many as it likes and silently halving occupancy -- the
// "performance cliff" of PMPP §6.3. The promise is checkable:
// cudaFuncGetAttributes reports the ceiling as maxThreadsPerBlock.
__global__ void __launch_bounds__(kMaxThreadsPerBlock, kMinBlocksPerSm)
    axpy(float a, const float* x, float* y, std::size_t n) {
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
// up to every limit -- threads per SM, blocks per SM, registers, shared
// memory. The runtime knows the kernel's resource usage, so it can answer.
double theoretical_occupancy(int block_size) {
  int blocks_per_sm = 0;
  CUDA_CHECK(
      cudaOccupancyMaxActiveBlocksPerMultiprocessor(&blocks_per_sm, axpy, block_size, 0));
  const int max_threads = device_attribute(cudaDevAttrMaxThreadsPerMultiProcessor);
  return static_cast<double>(blocks_per_sm) * block_size / max_threads;
}

struct LaunchConfig {
  unsigned grid;
  unsigned block;
};

// Asks the runtime for the block size that maximises occupancy for THIS
// kernel on THIS device, and for the smallest grid that fills the device at
// that block size. A grid-stride kernel can then be launched with just enough
// blocks to occupy every SM, rather than one block per 256 elements.
LaunchConfig choose_launch(std::size_t n) {
  int min_grid = 0;
  int block = 0;
  CUDA_CHECK(cudaOccupancyMaxPotentialBlockSize(&min_grid, &block, axpy, 0,
                                                kMaxThreadsPerBlock));
  const std::size_t needed = (n + static_cast<std::size_t>(block) - 1) / block;
  const std::size_t grid =
      std::max<std::size_t>(1, std::min<std::size_t>(needed, min_grid));
  return {static_cast<unsigned>(grid), static_cast<unsigned>(block)};
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
