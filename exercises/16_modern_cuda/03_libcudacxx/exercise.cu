// =============================================================================
//  16.03 -- libcu++: the standard library, on the device
// =============================================================================
//
//  Device code cannot use `std::vector`, `std::string`, `std::span` or
//  `std::atomic`: the standard library is compiled for the host, and nvcc
//  refuses to instantiate it for the GPU. What it can use is libcu++, the
//  third member of CCCL, which re-implements the useful subset of the
//  standard library under `cuda::std::` so that it works on both sides:
//
//      #include <cuda/std/span>     cuda::std::span<const float>
//      #include <cuda/std/array>    cuda::std::array<float, 4>
//      #include <cuda/atomic>       cuda::atomic_ref<int, cuda::thread_scope_device>
//
//  and, in the same spirit, `cuda::std::pair`, `tuple`, `optional`,
//  `numeric_limits`, `<cuda/std/cmath>`, type traits, and (from CCCL 2.x)
//  `mdspan` -- whose semantics changed in CUDA 13.1, which is why this
//  course does not use it.
//
//  Two of them matter for almost every kernel:
//
//   * A SPAN AS A KERNEL PARAMETER. `(const float* xs, std::size_t n)` is
//     two things that must agree; a `cuda::std::span<const float>` is one
//     thing that carries its own size (06.05). It is trivially copyable, so
//     it passes to a kernel by value like any other argument, and `xs[i]`
//     and `xs.size()` mean what they say inside the kernel.
//
//   * atomic_ref. Twelve thousand threads doing `++*count` is a data race
//     (10.02): most of the increments are lost, and the count is wrong by a
//     large, run-dependent amount. `cuda::atomic_ref<int, Scope>` wraps the
//     existing int and makes `fetch_add` atomic. The scope is new relative to
//     `std::atomic`: `thread_scope_device` promises visibility to threads on
//     this GPU only, which is cheaper than the default `thread_scope_system`
//     (host and other GPUs too) and is what a counter shared by one kernel
//     needs. The memory order is the same vocabulary as 10.07; for a counter
//     nobody reads until after the kernel, relaxed is enough.
//
//  Compared with the legacy `atomicAdd(count, 1)`: that still works and is
//  what CUB uses internally, but atomic_ref spells the scope and ordering out
//  where a reader can see them, and works on ordinary variables in structs.
//
//  SINCE THE BOOKS. PMPP (§12.2.4) discusses atomics only as a coming
//  hardware feature; the `cuda::` namespace did not exist. CCCL 3.0 removed
//  `thrust::pair`/`tuple`/`optional` in favour of these, so this header is
//  now the one to know.
//
//  TASK
//    Give `count_above` a span parameter instead of a pointer and a size, and
//    make its increment atomic.
//
//  NOTE  This exercise starts as a compile error: a static_assert in the
//        tests pins the kernel's signature. Once it compiles, the count is
//        still wrong until the increment is atomic.
//
//  RUN IT
//    ./mcpp test 16_03
//
// =============================================================================
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <cuda/atomic>
#include <cuda/std/array>
#include <cuda/std/span>
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

unsigned grid_for(std::size_t n) {
  return static_cast<unsigned>((n + kBlock - 1) / kBlock);
}

// Counts the elements above `threshold`.
//
// TODO (1): take `cuda::std::span<const float> xs` instead of a pointer and a
// size, and bounds-check against `xs.size()`.
//
// TODO (2): `++*count` from thousands of threads at once loses most of the
// increments. Wrap the int in a
// `cuda::atomic_ref<int, cuda::thread_scope_device>` and `fetch_add(1,
// cuda::std::memory_order_relaxed)`.
__global__ void count_above(const float* xs, std::size_t n, float threshold, int* count) {
  const std::size_t i = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
  if (i < n && xs[i] > threshold) {
    ++*count;
  }
}

// Weighted sum over groups of four consecutive elements. cuda::std::array is
// a value type, so the weights travel to the device as a kernel argument --
// no allocation, no copy call, no lifetime to manage.
__global__ void weighted_groups(cuda::std::span<const float> xs,
                                cuda::std::array<float, 4> w,
                                cuda::std::span<float> out) {
  const std::size_t g = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
  if (g < out.size()) {
    float total = 0.0F;
    for (std::size_t k = 0; k < w.size(); ++k) {
      total += w[k] * xs[4 * g + k];
    }
    out[g] = total;
  }
}

int count_above_on_device(const std::vector<float>& host, float threshold) {
  DeviceBuffer<float> xs(host.size());
  xs.upload(host);
  DeviceBuffer<int> count(1);
  CUDA_CHECK(cudaMemset(count.data(), 0, sizeof(int)));

  // TODO: pass `xs.span()` once the kernel takes a span.
  count_above<<<grid_for(host.size()), kBlock>>>(xs.data(), xs.size(), threshold,
                                                 count.data());
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());

  std::vector<int> result(1);
  count.download(result);
  return result[0];
}

std::vector<float> weighted_groups_on_device(const std::vector<float>& host,
                                             cuda::std::array<float, 4> weights) {
  DeviceBuffer<float> xs(host.size());
  xs.upload(host);
  DeviceBuffer<float> out(host.size() / 4);

  weighted_groups<<<grid_for(out.size()), kBlock>>>(xs.span(), weights, out.span());
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());

  std::vector<float> result(out.size());
  out.download(result);
  return result;
}

// --- tests -------------------------------------------------------------------

std::vector<float> uniform_floats(std::size_t n, unsigned seed) {
  std::mt19937 engine(seed);
  std::uniform_real_distribution<float> uniform(0.0F, 1.0F);
  std::vector<float> out(n);
  for (float& x : out) {
    x = uniform(engine);
  }
  return out;
}

TEST_CASE("the kernel takes a span, not a pointer and a size") {
  // A span cannot be handed the wrong length by a caller; a pointer/size pair
  // can, and 06.05 showed what that costs. The signature is part of the
  // specification here.
  static_assert(std::is_same_v<decltype(&count_above),
                               void (*)(cuda::std::span<const float>, float, int*)>);
  CHECK(true);
}

TEST_CASE("count_above agrees with the host") {
  const std::vector<float> xs = uniform_floats(std::size_t{1} << 22U, 3);
  const auto expected = static_cast<int>(
      std::count_if(xs.begin(), xs.end(), [](float x) { return x > 0.5F; }));
  CHECK(count_above_on_device(xs, 0.5F) == expected);
  CHECK(count_above_on_device(xs, 2.0F) == 0);
  CHECK(count_above_on_device(xs, -1.0F) == static_cast<int>(xs.size()));
}

TEST_CASE("count_above respects the size the span carries") {
  const std::vector<float> xs(1000, 1.0F);
  CHECK(count_above_on_device(xs, 0.0F) == 1000);
  const std::vector<float> odd(1001, 1.0F);
  CHECK(count_above_on_device(odd, 0.0F) == 1001);
}

TEST_CASE("weighted_groups applies the weights to each group of four") {
  std::vector<float> xs(4 * 5000);
  for (std::size_t i = 0; i < xs.size(); ++i) {
    xs[i] = static_cast<float>(i % 4 + 1); // 1 2 3 4 1 2 3 4 ...
  }
  const cuda::std::array<float, 4> weights{1.0F, 10.0F, 100.0F, 1000.0F};
  const std::vector<float> out = weighted_groups_on_device(xs, weights);
  REQUIRE(out.size() == 5000);
  bool ok = true;
  for (const float v : out) {
    ok = ok && v == 4321.0F;
  }
  CHECK(ok);
}
