// Solution -- 14.02 Error handling
#include <doctest/doctest.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <cuda_runtime.h>

// Every CUDA runtime call returns a cudaError_t. This turns an ignored code
// into an exception that names the failure and where it was checked. A macro
// rather than std::source_location (12.06): nvcc cannot yet compile
// `std::source_location::current()` in host code (NVIDIA bug 4173735), so the
// pre-C++20 spelling is the portable one here.
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
  // A launch returns nothing, so this is the only way to learn that the
  // configuration was rejected. It is not sticky: the device is still usable.
  CUDA_CHECK(cudaGetLastError());
  // Errors from INSIDE the kernel surface here, at the first call that waits
  // for it. Those are sticky: the context is gone and every later call fails.
  CUDA_CHECK(cudaDeviceSynchronize());

  std::vector<float> out(n);
  CUDA_CHECK(
      cudaMemcpy(out.data(), d_out.get(), n * sizeof(float), cudaMemcpyDeviceToHost));
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
