// Solution -- 16.02 CUB device algorithms
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

// Every cub::Device* algorithm is called twice. With a null temp-storage
// pointer it does no work and only writes how many bytes it needs; with a
// real pointer it runs. Forgetting the second call is the classic CUB bug,
// and it is silent: the first call returns cudaSuccess.
void sum_into(const int* d_in, int n, int* d_out) {
  std::size_t temp_bytes = 0;
  CUDA_CHECK(cub::DeviceReduce::Sum(nullptr, temp_bytes, d_in, d_out, n));
  DeviceBuffer<std::byte> temp(temp_bytes);
  CUDA_CHECK(cub::DeviceReduce::Sum(temp.data(), temp_bytes, d_in, d_out, n));
}

// Exclusive prefix sum: out[i] = in[0] + ... + in[i-1], out[0] = 0.
void exclusive_scan_into(const int* d_in, int n, int* d_out) {
  std::size_t temp_bytes = 0;
  CUDA_CHECK(cub::DeviceScan::ExclusiveSum(nullptr, temp_bytes, d_in, d_out, n));
  DeviceBuffer<std::byte> temp(temp_bytes);
  CUDA_CHECK(cub::DeviceScan::ExclusiveSum(temp.data(), temp_bytes, d_in, d_out, n));
}

// One sum per block, using CUB's block-level primitive inside our own kernel.
// BlockReduce is a collective: every thread of the block must call it, and
// only thread 0 receives a meaningful aggregate. Threads past the end of the
// data still take part -- they contribute zero.
constexpr int kBlock = 256;

__global__ void block_sums_kernel(const int* in, int n, int* out) {
  using BlockReduce = cub::BlockReduce<int, kBlock>;
  __shared__ typename BlockReduce::TempStorage temp;

  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  const int value = i < n ? in[i] : 0;
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
