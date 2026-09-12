// =============================================================================
//  14.02 -- Error handling
// =============================================================================
//
//  Every CUDA runtime call returns a `cudaError_t`, and the single most common
//  CUDA bug is not looking at it. `cudaMalloc` fails, the pointer stays null,
//  the copy fails, the launch does nothing, the copy back fails, and the
//  program prints whatever the output vector was initialised with. Nothing
//  crashes. (Chapter 05 argued that an error you can ignore is not an error
//  report; Core Guidelines E.2: "throw an exception to signal that a function
//  cannot perform its assigned task".)
//
//  There are three places an error can come from, and they behave
//  differently:
//
//   1. AN API CALL. `cudaMalloc`, `cudaMemcpy`, `cudaDeviceSynchronize`, ...
//      return their status directly. Wrap every one.
//
//   2. THE LAUNCH ITSELF. A launch is not a call and returns nothing. If the
//      configuration is impossible -- more threads per block than the device
//      allows, more shared memory than exists -- the launch is silently
//      dropped and the error is parked where `cudaGetLastError()` will find
//      it. Call it immediately after every launch. (`cudaPeekAtLastError()`
//      reads without clearing; the difference matters only when you want to
//      look twice.) A launch error is NOT sticky: the device carries on.
//
//   3. THE KERNEL'S EXECUTION. An out-of-bounds access inside a kernel is
//      reported by the NEXT call that has to wait for the device -- a
//      synchronise, a copy, another launch -- as `cudaErrorIllegalAddress`.
//      That error IS sticky: the context is destroyed and every later call in
//      the process returns the same error. There is no recovery short of
//      exiting. This is why the tests here provoke only a launch error.
//
//  The tool for all three is one function that throws:
//
//      void check(cudaError_t status, const char* file, int line);
//      #define CUDA_CHECK(expr) check((expr), __FILE__, __LINE__)
//
//  `cudaGetErrorName` gives the enumerator ("cudaErrorInvalidValue") and
//  `cudaGetErrorString` the sentence ("invalid argument"); an exception
//  should carry both, and the file and line. A macro, in a C++20 course:
//  12.06 replaced exactly this macro with std::source_location, but nvcc
//  cannot compile `std::source_location::current()` in host code (NVIDIA
//  bug 4173735, open since CUDA 12.1), so here the old spelling is the
//  portable one. It is a two-line macro that expands to a real function,
//  which is the least bad kind.
//
//  Motta (ch. 3, "A first running program") wraps every call the same way and
//  never shows one without it. Do likewise: the runtime is cheap to check and
//  expensive to guess about.
//
//  TASK
//    `fill_on_device` never looks at anything. Check the launch, the
//    synchronisation and the copy -- and look at the copy's direction while
//    you are there.
//
//  RUN IT
//    ./mcpp test 14_02
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <cuda_runtime.h>

// Every CUDA runtime call returns a cudaError_t. This turns an ignored code
// into an exception that names the failure and where it was checked.
void check(cudaError_t status, const char* file, int line) {
  if (status != cudaSuccess) {
    throw std::runtime_error(std::string{cudaGetErrorName(status)} + ": " +
                             cudaGetErrorString(status) + " at " + file + ":" +
                             std::to_string(line));
  }
}

#define CUDA_CHECK(expr) check((expr), __FILE__, __LINE__)

// A deleter, so that a std::unique_ptr can own device memory (03.07). This is
// enough for one function; 14.03 grows it into a class that also knows its
// size and can copy itself in and out.
struct CudaFree {
  void operator()(float* pointer) const noexcept {
    cudaFree(pointer);
  }
};
using DevicePtr = std::unique_ptr<float, CudaFree>;

DevicePtr device_alloc(std::size_t count) {
  float* raw = nullptr;
  CUDA_CHECK(cudaMalloc(&raw, count * sizeof(float)));
  return DevicePtr{raw};
}

__global__ void fill_kernel(float* out, std::size_t n, float value) {
  const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  if (i < n) {
    out[i] = value;
  }
}

// Fills n floats on the device and brings them back. `threads_per_block` is a
// parameter so that the tests can ask for an impossible launch.
std::vector<float> fill_on_device(std::size_t n, float value,
                                  unsigned threads_per_block) {
  const DevicePtr d_out = device_alloc(n);
  const unsigned blocks =
      static_cast<unsigned>((n + threads_per_block - 1) / threads_per_block);

  fill_kernel<<<blocks, threads_per_block>>>(d_out.get(), n, value);
  // TODO: the launch returns nothing. Ask cudaGetLastError() whether it was
  // accepted, and check the synchronisation too: that is where an error from
  // inside the kernel would surface.
  cudaDeviceSynchronize();

  std::vector<float> out(n);
  // TODO: check this call. It is also copying in the wrong direction -- the
  // return value would have told you so.
  cudaMemcpy(out.data(), d_out.get(), n * sizeof(float), cudaMemcpyHostToDevice);
  return out;
}

TEST_CASE("check turns an error code into an exception that says where") {
  CHECK_NOTHROW(CUDA_CHECK(cudaSuccess));
  CHECK_THROWS_WITH_AS(CUDA_CHECK(cudaErrorInvalidValue),
                       doctest::Contains("cudaErrorInvalidValue"), std::runtime_error);
  CHECK_THROWS_WITH_AS(CUDA_CHECK(cudaErrorInvalidValue),
                       doctest::Contains("exercise.cu:"), std::runtime_error);
}

TEST_CASE("a valid launch fills every element") {
  const std::vector<float> out = fill_on_device(1000, 3.5F, 256);
  REQUIRE(out.size() == 1000);
  bool all = true;
  for (float value : out) {
    all = all && (value == 3.5F);
  }
  CHECK(all);
}

TEST_CASE("an impossible launch configuration is reported, not ignored") {
  // 2048 threads per block is twice the hardware limit. The launch does
  // nothing, and without the check the function would return garbage.
  CHECK_THROWS_WITH_AS((void)fill_on_device(1000, 1.0F, 2048),
                       doctest::Contains("invalid configuration"), std::runtime_error);
}

TEST_CASE("a rejected launch is not sticky: the device still works") {
  try {
    (void)fill_on_device(16, 1.0F, 2048);
  } catch (const std::runtime_error&) {
  }
  const std::vector<float> out = fill_on_device(16, 2.0F, 32);
  CHECK(out.back() == 2.0F);
}
