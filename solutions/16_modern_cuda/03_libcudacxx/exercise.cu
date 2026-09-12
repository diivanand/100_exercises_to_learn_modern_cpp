// Solution -- 16.03 libcu++
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

// Counts the elements above `threshold`. The span carries its own size, so the
// kernel bounds-checks against the data it was actually given. The count is a
// single int that every qualifying thread increments: that is a data race
// unless the increment is atomic (10.06). atomic_ref wraps an existing int
// rather than requiring the memory to have been declared atomic, and
// thread_scope_device says the only threads that need to agree are those on
// this GPU -- a cheaper guarantee than the default system scope.
__global__ void count_above(cuda::std::span<const float> xs, float threshold,
                            int* count) {
  const std::size_t i = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
  if (i < xs.size() && xs[i] > threshold) {
    cuda::atomic_ref<int, cuda::thread_scope_device> ref(*count);
    ref.fetch_add(1, cuda::std::memory_order_relaxed);
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

  count_above<<<grid_for(host.size()), kBlock>>>(xs.span(), threshold, count.data());
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
