// Solution -- 15.02 Shared memory tiling
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

constexpr int kTile = 32;
// Each block is 32 x 8 threads and moves a 32 x 32 tile: every thread handles
// four elements. Fewer, busier threads than one-element-per-thread, and a
// block small enough that several fit on an SM.
constexpr int kRowsPerBlock = 8;

// The +1 is the whole point of this exercise. Shared memory is divided into 32
// banks, 4 bytes wide, interleaved: word `w` lives in bank `w % 32`. A 32 x 32
// tile has every column in the same bank, so the transposed read
// `tile[threadIdx.x][j]` makes 32 threads hit one bank -- a 32-way conflict,
// serialised into 32 accesses. Padding each row to 33 words shifts column `c`
// of row `r` into bank `(r * 33 + c) % 32 == (r + c) % 32`: distinct banks for
// distinct rows. It costs 128 bytes per tile and buys back the whole factor.
using Tile = float[kTile][kTile + 1];

// Transposes a `rows` x `cols` row-major matrix into a `cols` x `rows` one.
//
// Both global accesses are coalesced: the load reads a 32-wide row segment of
// the input, the store writes a 32-wide row segment of the output. The
// transpose itself happens in shared memory, where the access pattern costs
// nothing as long as the banks are not in conflict.
__global__ void transpose_tiled_kernel(const float* in, float* out, int rows, int cols) {
  __shared__ Tile tile;

  const int x = static_cast<int>(blockIdx.x) * kTile + static_cast<int>(threadIdx.x);
  const int y0 = static_cast<int>(blockIdx.y) * kTile;
  for (int j = static_cast<int>(threadIdx.y); j < kTile; j += kRowsPerBlock) {
    const int y = y0 + j;
    if (x < cols && y < rows) {
      tile[j][threadIdx.x] = in[y * cols + x];
    }
  }

  // Everything below reads what other threads wrote above.
  __syncthreads();

  // The block's tile lands transposed: output rows are input columns.
  const int tx = static_cast<int>(blockIdx.y) * kTile + static_cast<int>(threadIdx.x);
  const int ty0 = static_cast<int>(blockIdx.x) * kTile;
  for (int j = static_cast<int>(threadIdx.y); j < kTile; j += kRowsPerBlock) {
    const int ty = ty0 + j;
    if (tx < rows && ty < cols) {
      out[ty * rows + tx] = tile[threadIdx.x][j];
    }
  }
}

void transpose_tiled(const float* in, float* out, int rows, int cols) {
  const dim3 block(kTile, kRowsPerBlock);
  const dim3 grid((static_cast<unsigned>(cols) + kTile - 1) / kTile,
                  (static_cast<unsigned>(rows) + kTile - 1) / kTile);
  transpose_tiled_kernel<<<grid, block>>>(in, out, rows, cols);
  CUDA_CHECK(cudaGetLastError());
}

// --- tests -------------------------------------------------------------------

// The obvious transpose: reads coalesce, writes do not. One of the two global
// accesses is always strided, whichever way round you map the threads, and
// that is why the tile exists.
__global__ void transpose_naive_kernel(const float* in, float* out, int rows, int cols) {
  const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
  const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
  if (x < cols && y < rows) {
    out[x * rows + y] = in[y * cols + x];
  }
}

void transpose_naive(const float* in, float* out, int rows, int cols) {
  const dim3 block(32, 8);
  const dim3 grid((static_cast<unsigned>(cols) + 31) / 32,
                  (static_cast<unsigned>(rows) + 7) / 8);
  transpose_naive_kernel<<<grid, block>>>(in, out, rows, cols);
  CUDA_CHECK(cudaGetLastError());
}

std::vector<float> ramp(std::size_t n) {
  std::vector<float> v(n);
  std::iota(v.begin(), v.end(), 0.0F);
  return v;
}

TEST_CASE("the tile is padded against bank conflicts") {
  static_assert(sizeof(Tile) == sizeof(float) * kTile * (kTile + 1),
                "pad each tile row by one element so that a column read touches "
                "32 different banks");
  static_assert(kTile == 32, "a tile row is one warp wide");
  static_assert(kTile % kRowsPerBlock == 0, "each thread handles a whole number of rows");
  CHECK(true);
}

TEST_CASE("transposes a matrix whose sides are not multiples of the tile") {
  const int rows = 777;
  const int cols = 1000;
  const std::size_t n = static_cast<std::size_t>(rows) * cols;
  const std::vector<float> input = ramp(n);
  std::vector<float> output(n, -1.0F);

  DeviceBuffer<float> d_in(n);
  DeviceBuffer<float> d_out(n);
  d_in.upload(input);
  transpose_tiled(d_in.data(), d_out.data(), rows, cols);
  CUDA_CHECK(cudaDeviceSynchronize());
  d_out.download(output);

  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const float expected = input[static_cast<std::size_t>(r) * cols + c];
      const float actual = output[static_cast<std::size_t>(c) * rows + r];
      if (actual != expected) {
        CAPTURE(r);
        CAPTURE(c);
        REQUIRE(actual == expected);
      }
    }
  }
}

TEST_CASE("a single tile, and a matrix smaller than one tile") {
  for (const auto [rows, cols] :
       {std::pair{32, 32}, std::pair{5, 7}, std::pair{1, 100}}) {
    const std::size_t n = static_cast<std::size_t>(rows) * cols;
    const std::vector<float> input = ramp(n);
    std::vector<float> output(n, -1.0F);
    DeviceBuffer<float> d_in(n);
    DeviceBuffer<float> d_out(n);
    d_in.upload(input);
    transpose_tiled(d_in.data(), d_out.data(), rows, cols);
    CUDA_CHECK(cudaDeviceSynchronize());
    d_out.download(output);
    for (int r = 0; r < rows; ++r) {
      for (int c = 0; c < cols; ++c) {
        REQUIRE(output[static_cast<std::size_t>(c) * rows + r] ==
                input[static_cast<std::size_t>(r) * cols + c]);
      }
    }
  }
}

TEST_CASE("the tiled transpose beats the naive one") {
  const int rows = 4096;
  const int cols = 4096;
  const std::size_t n = static_cast<std::size_t>(rows) * cols;
  const std::vector<float> input = ramp(n);

  DeviceBuffer<float> d_in(n);
  DeviceBuffer<float> d_out(n);
  d_in.upload(input);

  const float naive =
      best_of(5, [&] { transpose_naive(d_in.data(), d_out.data(), rows, cols); });
  const float tiled =
      best_of(5, [&] { transpose_tiled(d_in.data(), d_out.data(), rows, cols); });
  CUDA_CHECK(cudaDeviceSynchronize());

  const double bytes = 2.0 * static_cast<double>(n) * sizeof(float);
  MESSAGE("naive: " << naive << " ms (" << bytes / naive / 1e6 << " GB/s), tiled: "
                    << tiled << " ms (" << bytes / tiled / 1e6 << " GB/s)");

  // Around 3x on an RTX 4090. Half of that is required, so that the test
  // measures the technique rather than the afternoon's clock speeds.
  CHECK(tiled * 1.5F < naive);
}
