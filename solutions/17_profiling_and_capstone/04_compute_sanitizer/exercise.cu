// Solution -- 17.04 compute-sanitizer
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cuda_runtime.h>

// --- the helpers from 14.02 and 14.03 ----------------------------------------

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

// -----------------------------------------------------------------------------

constexpr unsigned kBlock = 256;
constexpr int kMaxRadius = 3;

// Adds in[i - radius] and in[i + radius] to out[i], clamping both neighbours
// to the array. Clamping is the boundary policy; the alternative of reading
// past the ends is not a policy, it is undefined behaviour.
__global__ void add_neighbours_kernel(const float* in, float* out, std::size_t n,
                                      int radius) {
  const std::size_t r = static_cast<std::size_t>(radius);
  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    const std::size_t left = (i >= r) ? i - r : 0;
    const std::size_t right = (i + r < n) ? i + r : n - 1;
    out[i] += in[left] + in[right];
  }
}

// out[i] = sum over radius = 1..kMaxRadius of in[i - radius] + in[i + radius].
//
// Each launch adds one radius into `out`, so `out` must start at zero. The
// memset is asynchronous and stream-ordered, so it costs nothing to place it
// here rather than trust whatever the buffer held before.
void neighbour_sum(const float* in, float* out, std::size_t n) {
  CUDA_CHECK(cudaMemsetAsync(out, 0, n * sizeof(float)));
  const unsigned grid =
      static_cast<unsigned>(std::min<std::size_t>((n + kBlock - 1) / kBlock, 4096));
  for (int radius = 1; radius <= kMaxRadius; ++radius) {
    add_neighbours_kernel<<<grid, kBlock>>>(in, out, n, radius);
    CUDA_CHECK(cudaGetLastError());
  }
  CUDA_CHECK(cudaDeviceSynchronize());
}

// -----------------------------------------------------------------------------

namespace {

// An odd size that is not a multiple of anything, so the last block is
// partial and the last element is the last thread's problem.
constexpr std::size_t kN = 1'000'003;
// The input is placed in the middle of a larger buffer whose margins hold
// NaN. A read of in[-1] or in[n] therefore poisons the result instead of
// quietly returning whatever happened to be next in memory. (compute-sanitizer
// would report the read; the test makes it a wrong answer as well.)
constexpr std::size_t kGuard = 64;
constexpr float kSentinel = 12345.0F;

std::vector<float> make_input() {
  std::vector<float> in(kN);
  for (std::size_t i = 0; i < kN; ++i) {
    in[i] = static_cast<float>(i % 101) * 0.25F - 12.0F;
  }
  return in;
}

std::vector<float> reference(const std::vector<float>& in) {
  std::vector<float> out(in.size(), 0.0F);
  const std::size_t n = in.size();
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t r = 1; r <= static_cast<std::size_t>(kMaxRadius); ++r) {
      const std::size_t left = (i >= r) ? i - r : 0;
      const std::size_t right = (i + r < n) ? i + r : n - 1;
      out[i] += in[left] + in[right];
    }
  }
  return out;
}

// A device buffer holding NaN margins around the input.
DeviceBuffer<float> upload_guarded(const std::vector<float>& in) {
  std::vector<float> padded(in.size() + 2 * kGuard,
                            std::numeric_limits<float>::quiet_NaN());
  std::copy(in.begin(), in.end(), padded.begin() + static_cast<std::ptrdiff_t>(kGuard));
  DeviceBuffer<float> device(padded.size());
  device.upload(padded);
  return device;
}

int count_mismatches(const std::vector<float>& got, const std::vector<float>& want) {
  int mismatches = 0;
  for (std::size_t i = 0; i < got.size(); ++i) {
    // A NaN compares unequal to everything, including itself, so a poisoned
    // element is a mismatch by construction.
    if (!(std::fabs(got[i] - want[i]) <= 1e-4F)) {
      ++mismatches;
    }
  }
  return mismatches;
}

} // namespace

TEST_CASE("the neighbour sum matches the reference, including at both edges") {
  const std::vector<float> in = make_input();
  DeviceBuffer<float> guarded = upload_guarded(in);
  DeviceBuffer<float> out(kN);
  std::vector<float> zeros(kN, 0.0F);
  out.upload(zeros);

  neighbour_sum(guarded.data() + kGuard, out.data(), kN);

  std::vector<float> got(kN);
  out.download(got);
  const std::vector<float> want = reference(in);
  CHECK(count_mismatches(got, want) == 0);
  CHECK(std::isfinite(got.front()));
  CHECK(std::isfinite(got.back()));
}

TEST_CASE("the result does not depend on what the output buffer held before") {
  const std::vector<float> in = make_input();
  DeviceBuffer<float> guarded = upload_guarded(in);
  DeviceBuffer<float> out(kN);
  // A freshly allocated buffer often happens to be zero, which is how this
  // bug survives testing. Make sure it is not.
  std::vector<float> dirty(kN, kSentinel);
  out.upload(dirty);

  neighbour_sum(guarded.data() + kGuard, out.data(), kN);
  std::vector<float> first(kN);
  out.download(first);

  // Running it again into the same buffer must give the same answer.
  neighbour_sum(guarded.data() + kGuard, out.data(), kN);
  std::vector<float> second(kN);
  out.download(second);

  const std::vector<float> want = reference(in);
  CHECK(count_mismatches(first, want) == 0);
  CHECK(count_mismatches(second, want) == 0);
}
