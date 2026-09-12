// =============================================================================
//  14.01 -- Hello, kernel
// =============================================================================
//
//  Welcome to the CUDA track. The C++ is the same C++20 as the first thirteen
//  chapters; what changes is that some functions run on a second processor
//  with its own memory. CUDA calls the CPU and its memory the HOST and the GPU
//  and its memory the DEVICE, and a function that runs on the device is a
//  KERNEL. This file has one, and the rest of the chapter is about what it
//  takes to call it correctly.
//
//  A kernel is declared with `__global__` and called with a launch:
//
//      __global__ void add_kernel(const float* a, const float* b, float* out,
//                                 std::size_t n);
//      add_kernel<<<blocks, threads_per_block>>>(d_a, d_b, d_out, n);
//
//  The launch starts `blocks * threads_per_block` threads, every one running
//  the same function body (Kirk & Hwu, PMPP §3.5: single program, multiple
//  data). Each thread finds out which element it is responsible for from
//  three built-in variables (PMPP §4.2):
//
//      threadIdx.x     its index within its block          0 .. blockDim.x-1
//      blockIdx.x      its block's index within the grid   0 .. gridDim.x-1
//      blockDim.x      the number of threads per block
//
//  so its global index is `blockIdx.x * blockDim.x + threadIdx.x`. Threads in
//  a block are scheduled together on one streaming multiprocessor (SM), in
//  groups of 32 called warps; blocks are handed out to SMs as they free up.
//  You choose the block size (256 is a good default: a multiple of 32, and
//  the hardware limit is 1024 -- PMPP's 512 is the 2010 figure); the grid
//  size follows from the data.
//
//  Two things follow from "the grid is a whole number of blocks":
//
//   1. The grid is ROUNDED UP, so the last block usually has threads with
//      nothing to do. Every kernel guards against that: `if (i < n)`. Motta,
//      GPU Programming with C++ and CUDA, ch. 4 "Vector addition" works the
//      arithmetic through: 10,000 elements in blocks of 256 is 40 blocks and
//      240 idle threads.
//
//   2. A grid can also be SMALLER than the data. The GRID-STRIDE LOOP handles
//      both cases with one piece of code: a thread starts at its global index
//      and steps by the total number of threads in the grid,
//      `blockDim.x * gridDim.x`, until it runs off the end. With a large grid
//      the loop body runs once; with a small one it runs many times. It also
//      lets you pick the grid size for the hardware rather than the data,
//      which chapter 15 uses.
//
//  The device has its own memory (PMPP §3.4). `cudaMalloc` allocates in it,
//  `cudaMemcpy` moves bytes across with an explicit direction, `cudaFree`
//  releases it. None of this happens for you, and the device does not zero
//  what it hands out. Data movement usually dominates a small computation:
//  Motta's table in ch. 4 shows a 44x speed-up for the addition alone turning
//  into 1.0x once the copies are counted. The track's later chapters are
//  largely about that.
//
//  A launch is asynchronous: control returns to the host at once. Reading the
//  result needs `cudaDeviceSynchronize()` (or a copy, which waits). And a
//  launch returns nothing, so a mistake in the launch itself is only visible
//  through `cudaGetLastError()`, which 14.02 is about.
//
//  TASK
//    Make `blocks_for` round up, and make the kernel use its block index and
//    a grid-stride loop so that any grid size covers any input size.
//
//  RUN IT
//    ./mcpp test 14_01
//
// =============================================================================

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

// TODO: this thread only knows its index within its block, so every block
// writes the same first 256 elements and nothing else is ever touched. Compute
// the global index from blockIdx.x and blockDim.x, then turn the `if` into a
// grid-stride loop so that a grid with fewer threads than elements still
// covers all of them.
__global__ void add_kernel(const float* a, const float* b, float* out, std::size_t n) {
  const std::size_t i = threadIdx.x;
  if (i < n) {
    out[i] = a[i] + b[i];
  }
}

constexpr unsigned kBlock = 256;

// TODO: round up. Integer division truncates, so 1000 elements currently get
// 3 blocks of 256 and the last 232 elements have no thread.
unsigned blocks_for(std::size_t n) {
  return static_cast<unsigned>(n / kBlock);
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
