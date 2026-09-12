// =============================================================================
//  15.02 -- Shared memory tiling
// =============================================================================
//
//  Transposing a matrix is the simplest kernel that cannot be coalesced both
//  ways: if a warp reads a contiguous row segment of the input, it writes a
//  column of the output, 32 elements each `rows` floats apart. One of the two
//  accesses is strided whichever way you map the threads (15.01).
//
//  PMPP §5.3 ("A Strategy for Reducing Global Memory Traffic") gives the
//  general answer: partition the data into TILES that fit in shared memory,
//  move each tile between global and shared memory in the coalesced
//  direction, and do the awkward access pattern inside shared memory, where
//  it is close to free. Shared memory is on the SM, next to the L1 cache --
//  on Ada the two share 128 KB per SM, of which a block may use 48 KB by
//  default and up to 99 KB with cudaFuncSetAttribute -- and is around 20-30x
//  faster than DRAM (Cautaerts ch. 5, "Effectively using shared memory").
//  PMPP's G80 had 16 KB per SM; the strategy is the same, the budget is not.
//
//  So: a block loads a 32 x 32 tile row by row (coalesced), calls
//  `__syncthreads()` so that every thread's load is visible to every other
//  thread, then writes the tile out column by column of the tile -- which is
//  row by row of the OUTPUT, so that too is coalesced. Every thread in the
//  block must reach the barrier; a `__syncthreads()` inside a branch that
//  only some threads take is undefined behaviour (PMPP §4.3).
//
//  There is one trap, and it halves the speed of the naive version of this
//  kernel. Shared memory is divided into 32 BANKS, each 4 bytes wide, with
//  consecutive words in consecutive banks. A warp can service 32 accesses in
//  one go if they fall in 32 different banks (or read the same word). In a
//  `float tile[32][32]`, element [r][c] is word 32r + c, which is in bank c:
//  a whole column lives in ONE bank, so the transposed read `tile[tx][j]`
//  from 32 threads is a 32-way bank conflict, serialised into 32 trips
//  (Cautaerts ch. 5, "bank conflicts", measures nearly 2x on a 16k x 16k
//  transpose). Declaring the tile `[32][33]` moves [r][c] to word 33r + c,
//  bank (r + c) % 32, and the column's 32 elements spread over 32 banks. One
//  wasted word per row buys back the factor of two.
//
//  TASK
//    Pad the tile, then write `transpose_tiled_kernel` so that both global
//    accesses are coalesced and the shared-memory read is conflict-free.
//
//  NOTE  This exercise starts as a compile error: a static_assert in the
//        tests checks the padding.
//
//  RUN IT
//    ./mcpp test 15_02
//
// =============================================================================

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

// TODO: pad each row by one element. Element [r][c] of a [32][32] array is
// word 32r + c, which lives in bank c: an entire column shares one bank, and
// the transposed read below is a 32-way conflict.
using Tile = float[kTile][kTile];

// Transposes a `rows` x `cols` row-major matrix into a `cols` x `rows` one.
//
// TODO: this is the naive transpose with a shared-memory tile bolted on that
// does nothing useful: the store is still strided (`out[x * rows + y]`, 32
// threads writing `rows` floats apart). Rewrite it so that the block
//   1. loads a kTile x kTile tile of the input row by row (coalesced: x is the
//      column, which is the fast dimension of the input);
//   2. __syncthreads();
//   3. stores the tile transposed, row by row of the OUTPUT (coalesced: the
//      output row index is the input column index, so this block's output
//      tile starts at row blockIdx.x * kTile and column blockIdx.y * kTile).
// Each thread handles kTile / kRowsPerBlock rows of the tile: loop
// `for (int j = threadIdx.y; j < kTile; j += kRowsPerBlock)`.
// Keep the bounds guards -- the tests use sides that are not multiples of 32.
__global__ void transpose_tiled_kernel(const float* in, float* out, int rows, int cols) {
  __shared__ Tile tile;

  const int x = static_cast<int>(blockIdx.x) * kTile + static_cast<int>(threadIdx.x);
  const int y0 = static_cast<int>(blockIdx.y) * kTile;
  for (int j = static_cast<int>(threadIdx.y); j < kTile; j += kRowsPerBlock) {
    const int y = y0 + j;
    if (x < cols && y < rows) {
      tile[j][threadIdx.x] = in[y * cols + x];
      out[x * rows + y] = tile[j][threadIdx.x];
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
