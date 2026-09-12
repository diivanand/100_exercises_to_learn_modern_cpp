// Solution -- 16.07 Device lambdas and templates
#include <doctest/doctest.h>

#include <cmath>
#include <concepts>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cuda/std/span>
#include <cuda/std/type_traits>
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

  // A view of the whole buffer, in the vocabulary device code understands.
  cuda::std::span<T> span() noexcept {
    return cuda::std::span<T>{data_, count_};
  }
  cuda::std::span<const T> span() const noexcept {
    return cuda::std::span<const T>{data_, count_};
  }

private:
  T* data_ = nullptr;
  std::size_t count_ = 0;
};

constexpr unsigned kBlock = 256;

// A generic map: one kernel template for every element type and every
// operation. `f` is passed by value -- a functor or a device lambda is a
// small trivially copyable object, and kernel arguments are copied into
// constant memory at launch.
template <typename T, typename F>
__global__ void map_kernel(cuda::std::span<const T> in, cuda::std::span<T> out, F f) {
  const std::size_t i = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
  if (i < in.size()) {
    out[i] = f(in[i]);
  }
}

// The host-side entry point is where the constraint belongs (08.08): a caller
// who passes a std::vector<int> gets a one-line error at the call, not a page
// of template instantiation failures from inside the kernel.
template <std::floating_point T, typename F>
std::vector<T> map(const std::vector<T>& host, F f) {
  DeviceBuffer<T> in(host.size());
  in.upload(host);
  DeviceBuffer<T> out(host.size());

  // The kernel deduces T from a span<const T>, so it must be handed the const
  // view: a span<T> would make the deduction ambiguous.
  const cuda::std::span<const T> input = std::as_const(in).span();
  const auto grid = static_cast<unsigned>((host.size() + kBlock - 1) / kBlock);
  map_kernel<<<grid, kBlock>>>(input, out.span(), f);
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());

  std::vector<T> result(host.size());
  out.download(result);
  return result;
}

// `if constexpr` works in device code exactly as on the host (02.02): the
// branch not taken for this T is discarded, so float gets the fast intrinsic
// and double gets the accurate library call, from one function.
template <typename T>
__device__ T fast_exp(T x) {
  if constexpr (cuda::std::is_same_v<T, float>) {
    return __expf(x);
  } else {
    return ::exp(x);
  }
}

struct Exp {
  template <typename T>
  __device__ T operator()(T x) const {
    return fast_exp(x);
  }
};

// An extended lambda: `__device__` after the capture list says the closure's
// call operator is device code. It needs nvcc's --extended-lambda flag (on in
// cmake/Cuda.cmake) and must be written inside a named function, which is
// why it lives here rather than in a test.
std::vector<float> scale_all(const std::vector<float>& host, float factor) {
  return map(host, [factor] __device__(float x) { return x * factor; });
}

// --- tests -------------------------------------------------------------------

struct Halve {
  __device__ double operator()(double x) const {
    return x * 0.5;
  }
};

TEST_CASE("map applies a functor to every element") {
  const std::vector<double> in{2.0, 4.0, 6.0, 8.0, 10.0};
  CHECK(map(in, Halve{}) == std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0});
}

TEST_CASE("scale_all uses a device lambda that captures") {
  std::vector<float> in(100'003);
  for (std::size_t i = 0; i < in.size(); ++i) {
    in[i] = static_cast<float>(i);
  }
  const std::vector<float> out = scale_all(in, 3.0F);
  REQUIRE(out.size() == in.size());
  bool ok = true;
  for (std::size_t i = 0; i < in.size(); ++i) {
    ok = ok && out[i] == 3.0F * in[i];
  }
  CHECK(ok);
}

TEST_CASE("fast_exp picks the intrinsic for float and the library for double") {
  std::vector<float> xs_f;
  std::vector<double> xs_d;
  for (int i = -20; i <= 20; ++i) {
    xs_f.push_back(static_cast<float>(i) * 0.05F);
    xs_d.push_back(static_cast<double>(i) * 0.05);
  }
  const std::vector<float> ef = map(xs_f, Exp{});
  const std::vector<double> ed = map(xs_d, Exp{});
  for (std::size_t i = 0; i < xs_f.size(); ++i) {
    CHECK(ef[i] == doctest::Approx(std::exp(xs_f[i])).epsilon(1e-5));
    CHECK(ed[i] == doctest::Approx(std::exp(xs_d[i])).epsilon(1e-12));
  }
}

// A concept wrapping the call, so the question "does map accept this element
// type?" is asked in a dependent context and answered true or false rather
// than as a hard error (08.07, 08.08).
template <typename T>
concept Mappable = requires(const std::vector<T>& v) { map(v, Halve{}); };

TEST_CASE("map is constrained to floating-point elements") {
  // Without the constraint the kernel would happily instantiate for int and
  // the second assertion would fail.
  static_assert(Mappable<double>);
  static_assert(!Mappable<int>);
  CHECK(true);
}
