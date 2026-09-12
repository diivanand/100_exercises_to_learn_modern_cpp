// =============================================================================
//  16.02 -- CUB device algorithms
// =============================================================================
//
//  Thrust (16.01) is the convenient layer. CUB is the layer underneath it: the
//  same reductions, scans, sorts and histograms, but written to be embedded in
//  YOUR code -- as device-wide calls that take raw pointers and a stream, and
//  as block-wide and warp-wide primitives you use inside your own kernels.
//  Thrust's `reduce` is, internally, `cub::DeviceReduce`.
//
//  THE TWO-PHASE CALL. Every `cub::Device*` algorithm needs scratch memory,
//  and refuses to allocate it for you (an allocation is a synchronisation
//  point, 16.06, and CUB does not get to decide when you pay for one). So
//  each algorithm is called twice:
//
//      std::size_t bytes = 0;
//      cub::DeviceReduce::Sum(nullptr, bytes, d_in, d_out, n);   // 1: how much?
//      DeviceBuffer<std::byte> temp(bytes);
//      cub::DeviceReduce::Sum(temp.data(), bytes, d_in, d_out, n); // 2: do it
//
//  With a null temp pointer the call does NO WORK: it writes the size and
//  returns cudaSuccess. Forgetting the second call is the classic CUB mistake
//  and nothing reports it -- the output never changes. The tests here
//  pre-fill every output with a sentinel for exactly that reason.
//
//  BLOCK-LEVEL COLLECTIVES. `cub::BlockReduce<T, kBlock>` is the reduction
//  15.03 wrote by hand (warp shuffles, one shared slot per warp), packaged.
//  It is a collective: every thread in the block must call it, its shared
//  scratch is declared `__shared__ typename BlockReduce::TempStorage`, and
//  only thread 0 gets the aggregate back. Threads with no element still take
//  part and contribute the identity. (PMPP §6.1 is the reason it is worth
//  using a library here: the hand-written version is where divergence and
//  bank conflicts creep in.)
//
//  SINCE THE BOOKS. CUB is part of CCCL and lives at `<cub/cub.cuh>`. CCCL 3.0
//  (CUDA 13) removed the overloads with a trailing `bool debug_synchronous`
//  and the `LEGACY_PTX_ARCH` template parameter on `cub::Block*`; older
//  snippets that pass either will not compile. The five-argument calls used
//  here are the same in CUDA 12.4 and 13.
//
//  TASK
//    Complete the two device-wide calls, and guard the block kernel against
//    threads past the end of the data.
//
//  RUN IT
//    ./mcpp test 16_02
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

#include <cub/cub.cuh>
#include <cuda_runtime.h>

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

// TODO: this is only the size query. Nothing has been reduced yet: allocate
// `temp_bytes` of scratch (a DeviceBuffer<std::byte>) and call Sum again with
// it.
void sum_into(const int* d_in, int n, int* d_out) {
  std::size_t temp_bytes = 0;
  CUDA_CHECK(cub::DeviceReduce::Sum(nullptr, temp_bytes, d_in, d_out, n));
}

// Exclusive prefix sum: out[i] = in[0] + ... + in[i-1], out[0] = 0.
//
// TODO: the same two-phase call, with cub::DeviceScan::ExclusiveSum.
void exclusive_scan_into(const int* d_in, int n, int* d_out) {
  std::size_t temp_bytes = 0;
  CUDA_CHECK(cub::DeviceScan::ExclusiveSum(nullptr, temp_bytes, d_in, d_out, n));
}

// One sum per block, using CUB's block-level primitive inside our own kernel.
constexpr int kBlock = 256;

__global__ void block_sums_kernel(const int* in, int n, int* out) {
  using BlockReduce = cub::BlockReduce<int, kBlock>;
  __shared__ typename BlockReduce::TempStorage temp;

  // TODO: the last block runs past the end of the data. A thread with no
  // element must still call Sum (it is a collective -- every thread of the
  // block takes part) but contribute 0. Do not `return` early: a thread that
  // skips a collective leaves the others waiting on it.
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  const int value = in[i];
  const int aggregate = BlockReduce(temp).Sum(value);
  if (threadIdx.x == 0) {
    out[blockIdx.x] = aggregate;
  }
}

std::vector<int> block_sums(const DeviceBuffer<int>& in, int n) {
  const int blocks = (n + kBlock - 1) / kBlock;
  DeviceBuffer<int> out(static_cast<std::size_t>(blocks));
  block_sums_kernel<<<blocks, kBlock>>>(in.data(), n, out.data());
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());
  std::vector<int> host(static_cast<std::size_t>(blocks));
  out.download(host);
  return host;
}

// --- tests -------------------------------------------------------------------

// n is deliberately not a multiple of the block size, and the device copy is
// padded to one with a sentinel: a kernel that reads past `n` sees 1'000'000
// instead of undefined memory, so the mistake shows up as a wrong number
// rather than as luck. (compute-sanitizer would report the unpadded read;
// 17.04.)
constexpr int kSentinel = 1'000'000;

struct Input {
  std::vector<int> host;
  DeviceBuffer<int> device;
  int n = 0;
};

Input make_input(int n) {
  Input input;
  input.n = n;
  input.host.resize(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    input.host[static_cast<std::size_t>(i)] = i % 10;
  }
  const int padded = ((n + kBlock - 1) / kBlock) * kBlock;
  std::vector<int> padded_host(input.host);
  padded_host.resize(static_cast<std::size_t>(padded), kSentinel);
  input.device = DeviceBuffer<int>(static_cast<std::size_t>(padded));
  input.device.upload(padded_host);
  return input;
}

TEST_CASE("sum_into reduces the whole array") {
  const Input input = make_input((1 << 24) + 13);
  // The output slot starts as a sentinel, so "nothing was written" is
  // distinguishable from "the wrong thing was written".
  DeviceBuffer<int> out(1);
  const std::vector<int> sentinel{-1};
  out.upload(sentinel);

  sum_into(input.device.data(), input.n, out.data());
  CUDA_CHECK(cudaDeviceSynchronize());

  std::vector<int> result(1);
  out.download(result);
  const long long expected = std::accumulate(input.host.begin(), input.host.end(), 0LL);
  CHECK(result[0] == expected);
}

TEST_CASE("exclusive_scan_into produces prefix sums") {
  const Input input = make_input((1 << 20) + 5);
  DeviceBuffer<int> out(static_cast<std::size_t>(input.n));
  const std::vector<int> sentinel(static_cast<std::size_t>(input.n), -1);
  out.upload(sentinel);

  exclusive_scan_into(input.device.data(), input.n, out.data());
  CUDA_CHECK(cudaDeviceSynchronize());

  std::vector<int> result(static_cast<std::size_t>(input.n));
  out.download(result);
  std::vector<int> expected(static_cast<std::size_t>(input.n));
  std::exclusive_scan(input.host.begin(), input.host.end(), expected.begin(), 0);
  CHECK(result == expected);
}

TEST_CASE("block_sums gives one sum per block, including the partial last block") {
  const Input input = make_input(kBlock * 4096 + 37);
  const std::vector<int> sums = block_sums(input.device, input.n);

  const int blocks = (input.n + kBlock - 1) / kBlock;
  REQUIRE(static_cast<int>(sums.size()) == blocks);
  bool ok = true;
  for (int b = 0; b < blocks; ++b) {
    long long expected = 0;
    for (int i = b * kBlock; i < std::min((b + 1) * kBlock, input.n); ++i) {
      expected += input.host[static_cast<std::size_t>(i)];
    }
    ok = ok && sums[static_cast<std::size_t>(b)] == expected;
  }
  CHECK(ok);
  // The last block is where the bounds guard matters.
  long long last = 0;
  for (int i = (blocks - 1) * kBlock; i < input.n; ++i) {
    last += input.host[static_cast<std::size_t>(i)];
  }
  CHECK(sums.back() == last);
}
