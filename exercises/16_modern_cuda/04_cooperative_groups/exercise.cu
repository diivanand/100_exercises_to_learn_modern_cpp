// =============================================================================
//  16.04 -- Cooperative groups
// =============================================================================
//
//  Classic CUDA gives you two groups of threads and no way to name either:
//  the block (`__syncthreads()`) and the warp (`__shfl_down_sync(mask, ...)`,
//  where you supply the mask of participating lanes yourself, and get
//  undefined behaviour if you get it wrong -- Cautaerts ch. 5, "Exchanging
//  register data using warp shuffle instructions"). Cooperative groups make
//  the groups objects:
//
//      namespace cg = cooperative_groups;
//      cg::thread_block block = cg::this_thread_block();
//      cg::thread_block_tile<32> warp = cg::tiled_partition<32>(block);
//
//      warp.thread_rank()        // 0..31, my lane
//      warp.meta_group_rank()    // which warp of the block I am in
//      warp.meta_group_size()    // how many warps the block has
//      warp.shfl_down(v, 1)      // the shuffle, mask handled for you
//      warp.sync(); block.sync() // the barriers, scoped to the group
//      cg::reduce(warp, v, cg::plus<int>())   // <cooperative_groups/reduce.h>
//
//  The point is not brevity. A tile can be any power of two up to 32, so a
//  reduction over rows of 8 is a `tiled_partition<8>` and nothing else
//  changes; the group carries its own size, rank and membership, so the
//  arithmetic on threadIdx that every hand-written reduction gets slightly
//  wrong (15.03) is gone; and `cg::reduce` picks the fastest implementation
//  for the hardware -- on Ada, the `redux` instruction for integer sums.
//  `cg::coalesced_threads()` gives you the group of threads that are
//  currently active, which is how to do a warp-aggregated atomic correctly
//  inside a branch.
//
//  Grid-wide groups exist too (`cg::this_grid()`, `grid.sync()`), and are
//  what Cautaerts means by "Using cooperative groups instead of multiple
//  kernel launches" (ch. 5). They need a cooperative launch and relocatable
//  device code, which this project does not turn on; the block and tile
//  levels are where the everyday value is.
//
//  SINCE THE BOOKS. `cub::GridBarrier` was removed in CCCL 3.0 in favour of
//  cooperative groups; `multi_grid_group` was removed in CUDA 13. PMPP §6.1
//  ("More on Thread Execution") describes the warp-divergent reduction this
//  API is designed to make unnecessary.
//
//  TASK
//    `block_sum_kernel` reduces each warp but only ever writes the first
//    warp's sum. Add the second level. `row_max_kernel` partitions into
//    tiles of the wrong size; make the tile match the row.
//
//  RUN IT
//    ./mcpp test 16_04
//
// =============================================================================
#include <doctest/doctest.h>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
#include <cuda_runtime.h>

namespace cg = cooperative_groups;

// Reports a failed CUDA call with the file and line it was checked at (14.02).
void check(cudaError_t status, const char* file, int line) {
  if (status != cudaSuccess) {
    throw std::runtime_error(std::string{cudaGetErrorName(status)} + ": " +
                             cudaGetErrorString(status) + " at " + file + ":" +
                             std::to_string(line));
  }
}
#define CUDA_CHECK(expr) check((expr), __FILE__, __LINE__)

// Owns one device allocation: the rule of five around cudaMalloc/cudaFree (14.03).
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

constexpr int kBlock = 256;
constexpr int kWarp = 32;

// One sum per block.
//
// TODO: this reduces each warp and then writes the FIRST warp's sum as if it
// were the block's. Park each warp's sum in shared memory (lane 0 of each
// warp, indexed by `warp.meta_group_rank()`), `block.sync()`, and have the
// first warp reduce those partials -- lanes beyond `warp.meta_group_size()`
// contribute 0.
__global__ void block_sum_kernel(const int* in, int n, int* out) {
  cg::thread_block block = cg::this_thread_block();
  cg::thread_block_tile<kWarp> warp = cg::tiled_partition<kWarp>(block);

  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  const int value = i < n ? in[i] : 0;
  const int warp_sum = cg::reduce(warp, value, cg::plus<int>());

  if (threadIdx.x == 0) {
    out[blockIdx.x] = warp_sum;
  }
}

// The maximum of each row of eight.
//
// TODO: the tile is a whole warp, so the reduction mixes four rows together.
// A tile can be any power of two up to 32: partition into tiles of kRowWidth.
constexpr int kRowWidth = 8;

__global__ void row_max_kernel(const int* in, int rows, int* out) {
  cg::thread_block block = cg::this_thread_block();
  cg::thread_block_tile<kWarp> tile = cg::tiled_partition<kWarp>(block);

  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  const int row = i / kRowWidth;
  const int value = row < rows ? in[i] : INT_MIN;
  const int row_max = cg::reduce(tile, value, cg::greater<int>());
  if (tile.thread_rank() == 0 && row < rows) {
    out[row] = row_max;
  }
}

std::vector<int> block_sums(const std::vector<int>& host) {
  const int n = static_cast<int>(host.size());
  const int blocks = (n + kBlock - 1) / kBlock;
  DeviceBuffer<int> in(host.size());
  in.upload(host);
  DeviceBuffer<int> out(static_cast<std::size_t>(blocks));
  block_sum_kernel<<<blocks, kBlock>>>(in.data(), n, out.data());
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());
  std::vector<int> result(out.size());
  out.download(result);
  return result;
}

std::vector<int> row_maxes(const std::vector<int>& host) {
  const int rows = static_cast<int>(host.size() / kRowWidth);
  const int blocks = (rows * kRowWidth + kBlock - 1) / kBlock;
  DeviceBuffer<int> in(host.size());
  in.upload(host);
  DeviceBuffer<int> out(static_cast<std::size_t>(rows));
  row_max_kernel<<<blocks, kBlock>>>(in.data(), rows, out.data());
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());
  std::vector<int> result(out.size());
  out.download(result);
  return result;
}

// --- tests -------------------------------------------------------------------

TEST_CASE("block_sums adds every warp of the block, not just the first") {
  const int n = kBlock * 3000 + 100;
  std::vector<int> host(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    host[static_cast<std::size_t>(i)] = i % 7;
  }
  const std::vector<int> sums = block_sums(host);

  const int blocks = (n + kBlock - 1) / kBlock;
  REQUIRE(static_cast<int>(sums.size()) == blocks);
  bool ok = true;
  for (int b = 0; b < blocks; ++b) {
    int expected = 0;
    for (int i = b * kBlock; i < std::min((b + 1) * kBlock, n); ++i) {
      expected += host[static_cast<std::size_t>(i)];
    }
    ok = ok && sums[static_cast<std::size_t>(b)] == expected;
  }
  CHECK(ok);
}

TEST_CASE("a block whose values live in one warp only") {
  // 32 non-zero values then zeros: the first warp's sum is the block's sum,
  // so a kernel that forgot the second level would pass. The previous test
  // is the one that catches that; this one pins the easy case.
  std::vector<int> host(kBlock, 0);
  for (int i = 0; i < kWarp; ++i) {
    host[static_cast<std::size_t>(i)] = 1;
  }
  CHECK(block_sums(host) == std::vector<int>{kWarp});
}

TEST_CASE("row_maxes does not mix neighbouring rows") {
  constexpr int kRows = 10'000;
  std::mt19937 engine(11);
  std::uniform_int_distribution<int> dist(-1000, 1000);
  std::vector<int> host(kRows * kRowWidth);
  for (int& x : host) {
    x = dist(engine);
  }
  // Make row 0 all small and row 1 contain the global maximum, so any mixing
  // of adjacent rows is caught at the very first output.
  for (int k = 0; k < kRowWidth; ++k) {
    host[static_cast<std::size_t>(k)] = -5000;
  }
  host[kRowWidth + 3] = 9999;

  const std::vector<int> maxes = row_maxes(host);
  REQUIRE(static_cast<int>(maxes.size()) == kRows);
  CHECK(maxes[0] == -5000);
  CHECK(maxes[1] == 9999);
  bool ok = true;
  for (int r = 0; r < kRows; ++r) {
    const auto first = host.begin() + static_cast<std::ptrdiff_t>(r) * kRowWidth;
    ok = ok && maxes[static_cast<std::size_t>(r)] ==
                   *std::max_element(first, first + kRowWidth);
  }
  CHECK(ok);
}
