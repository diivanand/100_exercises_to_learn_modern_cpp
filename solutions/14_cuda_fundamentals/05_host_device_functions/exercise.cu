// Solution -- 14.05 Host and device functions
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
// --expt-relaxed-constexpr (which this project turns on) that is fine: it is
// usable from device code as if it were __host__ __device__, and it remains
// usable in constant expressions on the host.
constexpr float clamp01(float x) {
  return x < 0.0F ? 0.0F : (x > 1.0F ? 1.0F : x);
}

// Both spaces, one definition. The kernel and the CPU reference below call the
// same function, so they cannot drift apart.
__host__ __device__ float smoothstep(float edge0, float edge1, float x) {
  const float t = clamp01((x - edge0) / (edge1 - edge0));
  return t * t * (3.0F - 2.0F * t);
}

// A template is instantiated wherever it is used. Given both qualifiers it
// can be instantiated for the host with double and for the device with float
// from the same source.
template <typename T>
__host__ __device__ T mix(T a, T b, T t) {
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
