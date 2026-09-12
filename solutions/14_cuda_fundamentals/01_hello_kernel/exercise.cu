// Solution -- 14.01 Hello, kernel
#include <doctest/doctest.h>

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <cuda_runtime.h>

// Turns an ignored error code into an exception. This is the crude form; 14.02
// replaces it with something that also says where the failure happened.
void require(cudaError_t status, const char* what) {
  if (status != cudaSuccess) {
    throw std::runtime_error(std::string{what} + ": " + cudaGetErrorString(status));
  }
}

// One thread per element would need a grid exactly as large as the data. The
// grid-stride loop lets ANY grid size cover ANY data size: each thread starts
// at its global index and steps by the total number of threads in the grid.
// With a big enough grid the loop runs once per thread; with a small one it
// runs several times. Both are correct, and only the second is testable
// without a very large input.
__global__ void add_kernel(const float* a, const float* b, float* out, std::size_t n) {
  const std::size_t stride = static_cast<std::size_t>(blockDim.x) * gridDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    out[i] = a[i] + b[i];
  }
}

constexpr unsigned kBlock = 256;

// Enough blocks for one thread per element, rounded up. Integer division
// truncates, so the `+ kBlock - 1` is what makes 1000 elements need 4 blocks
// of 256 rather than 3.
unsigned blocks_for(std::size_t n) {
  return static_cast<unsigned>((n + kBlock - 1) / kBlock);
}

// Launches the kernel with an explicit grid size, so a test can ask for fewer
// blocks than elements and prove the grid-stride loop covers the rest.
void launch_add(const float* d_a, const float* d_b, float* d_out, std::size_t n,
                unsigned blocks) {
  add_kernel<<<blocks, kBlock>>>(d_a, d_b, d_out, n);
  require(cudaGetLastError(), "add_kernel launch");
}

// The whole round trip: allocate on the device, copy in, compute, copy out,
// free. Every later exercise does some version of this; 14.03 makes the
// allocation part safe.
std::vector<float> add_on_device(std::span<const float> a, std::span<const float> b,
                                 unsigned blocks) {
  if (a.size() != b.size()) {
    throw std::invalid_argument("add_on_device: sizes differ");
  }
  const std::size_t n = a.size();
  const std::size_t bytes = n * sizeof(float);

  float* d_a = nullptr;
  float* d_b = nullptr;
  float* d_out = nullptr;
  require(cudaMalloc(&d_a, bytes), "cudaMalloc a");
  require(cudaMalloc(&d_b, bytes), "cudaMalloc b");
  require(cudaMalloc(&d_out, bytes), "cudaMalloc out");

  require(cudaMemcpy(d_a, a.data(), bytes, cudaMemcpyHostToDevice), "copy a");
  require(cudaMemcpy(d_b, b.data(), bytes, cudaMemcpyHostToDevice), "copy b");
  // Device memory is not zeroed for you. Setting every byte to 0xFF makes each
  // float a NaN, so an element the kernel never writes cannot pass a test by
  // happening to contain the right value.
  require(cudaMemset(d_out, 0xFF, bytes), "memset out");

  launch_add(d_a, d_b, d_out, n, blocks);
  require(cudaDeviceSynchronize(), "add_kernel execution");

  std::vector<float> out(n);
  require(cudaMemcpy(out.data(), d_out, bytes, cudaMemcpyDeviceToHost), "copy out");

  cudaFree(d_a);
  cudaFree(d_b);
  cudaFree(d_out);
  return out;
}

std::vector<float> add_on_device(std::span<const float> a, std::span<const float> b) {
  return add_on_device(a, b, blocks_for(a.size()));
}

namespace {

std::vector<float> ramp(std::size_t n, float offset) {
  std::vector<float> values(n);
  for (std::size_t i = 0; i < n; ++i) {
    values[i] = static_cast<float>(i % 1024) + offset;
  }
  return values;
}

bool all_sums(std::span<const float> a, std::span<const float> b,
              std::span<const float> out) {
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (out[i] != a[i] + b[i]) {
      return false;
    }
  }
  return true;
}

} // namespace

TEST_CASE("blocks_for rounds up") {
  CHECK(blocks_for(0) == 0);
  CHECK(blocks_for(1) == 1);
  CHECK(blocks_for(256) == 1);
  CHECK(blocks_for(257) == 2);
  CHECK(blocks_for(1000) == 4);
}

TEST_CASE("every element is added, not just the first block's") {
  const std::size_t n = (1U << 20) + 3; // not a multiple of the block size
  const std::vector<float> a = ramp(n, 0.5F);
  const std::vector<float> b = ramp(n, 0.25F);

  const std::vector<float> out = add_on_device(a, b);
  REQUIRE(out.size() == n);
  CHECK(all_sums(a, b, out));
  CHECK(out[n - 1] == a[n - 1] + b[n - 1]);
}

TEST_CASE("a grid smaller than the data still covers it") {
  // Four blocks of 256 threads for a million elements: each thread must loop.
  const std::size_t n = 1U << 20;
  const std::vector<float> a = ramp(n, 1.0F);
  const std::vector<float> b = ramp(n, 2.0F);

  const std::vector<float> out = add_on_device(a, b, 4);
  CHECK(all_sums(a, b, out));
}

TEST_CASE("a grid larger than the data does not write past it") {
  const std::size_t n = 1000;
  const std::vector<float> a = ramp(n, 0.0F);
  const std::vector<float> b = ramp(n, 0.0F);

  // The last block has 24 threads with nothing to do; they must do nothing.
  const std::vector<float> out = add_on_device(a, b, blocks_for(n));
  CHECK(all_sums(a, b, out));
}

TEST_CASE("mismatched inputs are rejected before touching the device") {
  const std::vector<float> a(10, 1.0F);
  const std::vector<float> b(11, 1.0F);
  CHECK_THROWS_AS((void)add_on_device(a, b), std::invalid_argument);
}
