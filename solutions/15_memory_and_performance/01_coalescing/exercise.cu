// Solution -- 15.01 Memory coalescing
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cuda_runtime.h>

// From 14.02 and 14.03.
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

// Event-based timing (Motta ch. 4). The GPU's own clock, so an asynchronous
// launch cannot fool it; explained properly in 17.01.
class GpuTimer {
public:
  GpuTimer() {
    CUDA_CHECK(cudaEventCreate(&start_));
    CUDA_CHECK(cudaEventCreate(&stop_));
  }
  ~GpuTimer() {
    cudaEventDestroy(start_);
    cudaEventDestroy(stop_);
  }
  GpuTimer(const GpuTimer&) = delete;
  GpuTimer& operator=(const GpuTimer&) = delete;
  void start() {
    CUDA_CHECK(cudaEventRecord(start_, nullptr));
  }
  float stop() {
    CUDA_CHECK(cudaEventRecord(stop_, nullptr));
    CUDA_CHECK(cudaEventSynchronize(stop_));
    float ms = 0.0F;
    CUDA_CHECK(cudaEventElapsedTime(&ms, start_, stop_));
    return ms;
  }

private:
  cudaEvent_t start_{};
  cudaEvent_t stop_{};
};

// Best of N, after one warm-up run: the minimum is the least-disturbed run.
template <typename F>
float best_of(int repetitions, F&& work) {
  GpuTimer timer;
  float best = 1e30F;
  work();
  CUDA_CHECK(cudaDeviceSynchronize());
  for (int i = 0; i < repetitions; ++i) {
    timer.start();
    work();
    best = std::min(best, timer.stop());
  }
  return best;
}

constexpr unsigned kBlockX = 32;
constexpr unsigned kBlockY = 8;

// threadIdx.x walks along a ROW, which is the direction in which consecutive
// elements are adjacent in memory. The 32 threads of a warp therefore read 32
// consecutive floats -- 128 contiguous bytes, four 32-byte sectors, nothing
// fetched that is not used -- and write the same way.
__global__ void scale_matrix(const float* in, float* out, int width, int height,
                             float factor) {
  const int col = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
  const int row = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
  if (row < height && col < width) {
    out[row * width + col] = in[row * width + col] * factor;
  }
}

// The grid is shaped like the matrix: x across the columns, y down the rows.
// Keeping the two in step is what makes the kernel above coalesce.
void launch_scale(const float* in, float* out, int width, int height, float factor) {
  const dim3 block(kBlockX, kBlockY);
  const dim3 grid((static_cast<unsigned>(width) + kBlockX - 1) / kBlockX,
                  (static_cast<unsigned>(height) + kBlockY - 1) / kBlockY);
  scale_matrix<<<grid, block>>>(in, out, width, height, factor);
  CUDA_CHECK(cudaGetLastError());
}

// --- tests -------------------------------------------------------------------

// What the course knows to be the right answer, kept inside the test so that
// the timing comparison is against a fixed yardstick rather than a number.
// (Kernels stay at namespace scope: a __global__ function must have external
// linkage under separate compilation, and it costs nothing to keep the habit.)
__global__ void reference_scale(const float* in, float* out, int width, int height,
                                float factor) {
  const int col = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
  const int row = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
  if (row < height && col < width) {
    out[row * width + col] = in[row * width + col] * factor;
  }
}

void launch_reference(const float* in, float* out, int width, int height, float factor) {
  const dim3 block(32, 8);
  const dim3 grid((static_cast<unsigned>(width) + 31) / 32,
                  (static_cast<unsigned>(height) + 7) / 8);
  reference_scale<<<grid, block>>>(in, out, width, height, factor);
  CUDA_CHECK(cudaGetLastError());
}

std::vector<float> ramp(std::size_t n) {
  std::vector<float> v(n);
  std::iota(v.begin(), v.end(), 0.0F);
  return v;
}

TEST_CASE("every element is scaled, including the ragged edges") {
  // Neither dimension is a multiple of the block, so a kernel that forgets its
  // bounds guard, or maps the grid the wrong way round, writes the wrong cells.
  const int width = 1000;
  const int height = 777;
  const std::size_t n = static_cast<std::size_t>(width) * height;
  const std::vector<float> input = ramp(n);
  std::vector<float> output(n, -1.0F);

  DeviceBuffer<float> d_in(n);
  DeviceBuffer<float> d_out(n);
  d_in.upload(input);
  launch_scale(d_in.data(), d_out.data(), width, height, 3.0F);
  CUDA_CHECK(cudaDeviceSynchronize());
  d_out.download(output);

  for (std::size_t i = 0; i < n; ++i) {
    if (output[i] != input[i] * 3.0F) {
      CAPTURE(i);
      REQUIRE(output[i] == input[i] * 3.0F);
    }
  }
}

TEST_CASE("the access pattern is coalesced") {
  // 4096 x 4096 floats: 64 MB in, 64 MB out. Wide rows make a strided pattern
  // hurt as much as it does in practice.
  const int width = 4096;
  const int height = 4096;
  const std::size_t n = static_cast<std::size_t>(width) * height;
  const std::vector<float> input = ramp(n);

  DeviceBuffer<float> d_in(n);
  DeviceBuffer<float> d_out(n);
  d_in.upload(input);

  const float yours =
      best_of(5, [&] { launch_scale(d_in.data(), d_out.data(), width, height, 0.5F); });
  const float reference = best_of(
      5, [&] { launch_reference(d_in.data(), d_out.data(), width, height, 0.5F); });
  CUDA_CHECK(cudaDeviceSynchronize());

  const double bytes = 2.0 * static_cast<double>(n) * sizeof(float);
  MESSAGE("yours: " << yours << " ms (" << bytes / yours / 1e6 << " GB/s), coalesced "
                    << "reference: " << reference << " ms (" << bytes / reference / 1e6
                    << " GB/s)");

  // A column-strided kernel is 8-10x slower than this on an RTX 4090. The
  // bound is deliberately loose: it is here to catch the pattern, not to
  // grade the hardware.
  CHECK(yours < reference * 3.0F);
}

TEST_CASE("the block is shaped for coalescing") {
  // A warp is 32 threads with consecutive threadIdx.x. Making the block at
  // least 32 wide in x is what lets a whole warp read one contiguous row
  // segment.
  static_assert(kBlockX >= 32, "threadIdx.x must span at least one warp");
  static_assert(kBlockX * kBlockY <= 1024, "a block holds at most 1024 threads");
  CHECK(true);
}
