// =============================================================================
//  14.05 -- Host and device functions
// =============================================================================
//
//  Every function in a CUDA program has an EXECUTION SPACE, given by a
//  qualifier on its declaration (PMPP §3.5; CUDA C++ Programming Guide,
//  "Function Execution Space Specifiers"):
//
//      __global__             a kernel: runs on the device, launched from the
//                             host, returns void
//      __device__             runs on the device, callable only from device
//                             code
//      __host__               runs on the host; the default, so you never
//                             write it alone
//      __host__ __device__    compiled twice, once for each
//
//  The compiler enforces the boundary. A `__host__` function called from a
//  kernel is an error, not a warning, because there is no host code on the
//  device to jump to. This exercise starts with exactly that error.
//
//  `__host__ __device__` is the interesting one. A helper that computes
//  something -- a colour ramp, a distance, a hash -- is usually wanted on
//  both sides: in the kernel, and in the CPU reference you test the kernel
//  against. Writing it once with both qualifiers means the two cannot drift
//  apart. It applies to templates too: `template <typename T> __host__
//  __device__ T mix(T, T, T)` is instantiated for the host with double and
//  for the device with float from one definition.
//
//  CONSTEXPR is a special case. A constexpr function has no qualifier, and
//  nvcc's flag `--expt-relaxed-constexpr` (on in this project, see
//  cmake/Cuda.cmake) lets device code call it anyway. So a helper that is
//  `constexpr` is usable in a kernel AND in a static_assert on the host.
//  This is what most of the standard library's `constexpr` maths would need
//  to be usable on the device; without the flag they are not.
//
//  What cannot cross: anything that needs the host. std::vector, std::string,
//  exceptions, iostream, heap allocation through new (well, `new` exists on
//  the device, but from a small fixed heap and you will not want it). A
//  device function takes pointers and values, and 16.03 shows the
//  `cuda::std::` library that supplies device-side versions of the
//  containers and algorithms that make sense there.
//
//  One thing to notice in the last test: the device and the host disagree
//  in the last bit. nvcc fuses `a * b + c` into a single fused multiply-add
//  by default (`--fmad=true`), rounding once instead of twice; the host
//  compiler may or may not. Neither is wrong -- PMPP ch. 7 is the chapter on
//  why -- but exact float comparison between the two is a mistake.
//
//  TASK
//    Make `smoothstep` and `mix` callable from the kernel.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 14_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cuda_runtime.h>

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
  void upload(std::span<const T> host) {
    CUDA_CHECK(cudaMemcpy(data_, host.data(), host.size_bytes(), cudaMemcpyHostToDevice));
  }
  void download(std::span<T> host) const {
    CUDA_CHECK(cudaMemcpy(host.data(), data_, host.size_bytes(), cudaMemcpyDeviceToHost));
  }

private:
  T* data_ = nullptr;
  std::size_t count_ = 0;
};

// A constexpr function has no execution-space qualifier, and with
// --expt-relaxed-constexpr that is fine: it is usable from device code, and
// it remains usable in constant expressions on the host. Leave it alone.
constexpr float clamp01(float x) {
  return x < 0.0F ? 0.0F : (x > 1.0F ? 1.0F : x);
}

// TODO: this is a host function (the default), and the kernel below calls it.
// Give it both execution spaces so that the kernel and the CPU reference use
// the same definition.
float smoothstep(float edge0, float edge1, float x) {
  const float t = clamp01((x - edge0) / (edge1 - edge0));
  return t * t * (3.0F - 2.0F * t);
}

// TODO: the same for this template. It is instantiated for the host with
// double and int in the tests, and for the device with float in the kernel.
template <typename T>
T mix(T a, T b, T t) {
  return a + (b - a) * t;
}

__global__ void shade_kernel(const float* in, float* out, std::size_t n, float lo,
                             float hi) {
  const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  if (i < n) {
    out[i] = mix(0.0F, 1.0F, smoothstep(lo, hi, in[i]));
  }
}

std::vector<float> shade_on_device(std::span<const float> in, float lo, float hi) {
  DeviceBuffer<float> d_in(in.size());
  DeviceBuffer<float> d_out(in.size());
  d_in.upload(in);

  constexpr unsigned kBlock = 256;
  const unsigned blocks = static_cast<unsigned>((in.size() + kBlock - 1) / kBlock);
  shade_kernel<<<blocks, kBlock>>>(d_in.data(), d_out.data(), in.size(), lo, hi);
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());

  std::vector<float> out(in.size());
  d_out.download(out);
  return out;
}

// The same computation on the host, through the same functions.
std::vector<float> shade_on_host(std::span<const float> in, float lo, float hi) {
  std::vector<float> out(in.size());
  for (std::size_t i = 0; i < in.size(); ++i) {
    out[i] = mix(0.0F, 1.0F, smoothstep(lo, hi, in[i]));
  }
  return out;
}

TEST_CASE("clamp01 is still a constant expression on the host") {
  static_assert(clamp01(-3.0F) == 0.0F);
  static_assert(clamp01(0.25F) == 0.25F);
  static_assert(clamp01(7.0F) == 1.0F);
  CHECK(clamp01(0.5F) == 0.5F);
}

TEST_CASE("smoothstep and mix work on the host, for more than one type") {
  CHECK(smoothstep(0.0F, 1.0F, 0.0F) == 0.0F);
  CHECK(smoothstep(0.0F, 1.0F, 1.0F) == 1.0F);
  CHECK(smoothstep(0.0F, 1.0F, 0.5F) == doctest::Approx(0.5F));
  CHECK(mix(10.0, 20.0, 0.25) == doctest::Approx(12.5));
  CHECK(mix(1, 5, 1) == 5);
}

TEST_CASE("the device computes the same thing as the host") {
  const std::size_t n = 1U << 16;
  std::vector<float> in(n);
  for (std::size_t i = 0; i < n; ++i) {
    in[i] = static_cast<float>(i) / static_cast<float>(n) * 3.0F - 1.0F; // -1 .. 2
  }

  const std::vector<float> host = shade_on_host(in, 0.0F, 1.0F);
  const std::vector<float> device = shade_on_device(in, 0.0F, 1.0F);
  REQUIRE(device.size() == host.size());

  // The device may fuse a multiply and an add into one rounded operation
  // (--fmad is on by default), so the two are equal to within a few ulp,
  // not bit for bit. Chapter 17 comes back to this.
  float worst = 0.0F;
  for (std::size_t i = 0; i < n; ++i) {
    worst = std::fmax(worst, std::fabs(device[i] - host[i]));
  }
  CHECK(worst < 1e-6F);
  CHECK(device.front() == 0.0F);
  CHECK(device.back() == 1.0F);
}
