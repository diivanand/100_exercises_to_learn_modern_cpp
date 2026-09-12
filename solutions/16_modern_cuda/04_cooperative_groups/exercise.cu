// Solution -- 16.04 Cooperative groups
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

// One sum per block, in two levels. Level 1: each warp reduces its 32 values
// with cg::reduce, which compiles to shuffles (or the redux instruction on
// sm_80+). Level 2: lane 0 of every warp parks its sum in shared memory, the
// block synchronises, and the first warp reduces those eight partials. The
// tile object knows which warp it is (meta_group_rank) and how many there are
// (meta_group_size), so nothing here is derived from threadIdx by hand.
__global__ void block_sum_kernel(const int* in, int n, int* out) {
  cg::thread_block block = cg::this_thread_block();
  cg::thread_block_tile<kWarp> warp = cg::tiled_partition<kWarp>(block);

  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  const int value = i < n ? in[i] : 0;
  const int warp_sum = cg::reduce(warp, value, cg::plus<int>());

  __shared__ int warp_sums[kBlock / kWarp];
  if (warp.thread_rank() == 0) {
    warp_sums[warp.meta_group_rank()] = warp_sum;
  }
  block.sync();

  if (warp.meta_group_rank() == 0) {
    const int partial =
        warp.thread_rank() < warp.meta_group_size() ? warp_sums[warp.thread_rank()] : 0;
    const int total = cg::reduce(warp, partial, cg::plus<int>());
    if (warp.thread_rank() == 0) {
      out[blockIdx.x] = total;
    }
  }
}

// The maximum of each row of eight. A tile of 8 threads is a group smaller
// than a warp; the reduction only mixes values within the tile, so each row
// gets its own answer. Rows past the end contribute INT_MIN and write nothing.
constexpr int kRowWidth = 8;

__global__ void row_max_kernel(const int* in, int rows, int* out) {
  cg::thread_block block = cg::this_thread_block();
  cg::thread_block_tile<kRowWidth> tile = cg::tiled_partition<kRowWidth>(block);

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
