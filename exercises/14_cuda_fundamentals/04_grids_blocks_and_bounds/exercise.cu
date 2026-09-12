// =============================================================================
//  14.04 -- Grids, blocks and bounds
// =============================================================================
//
//  Blocks and grids are three-dimensional (PMPP §4.1). A launch takes two
//  `dim3` values instead of two integers, and each thread gets a `.y` and
//  `.z` coordinate as well as `.x`:
//
//      const dim3 block(16, 16);              // 256 threads, 16 by 16
//      const dim3 grid((w + 15) / 16, (h + 15) / 16);
//      kernel<<<grid, block>>>(...);
//
//      col = blockIdx.x * blockDim.x + threadIdx.x;
//      row = blockIdx.y * blockDim.y + threadIdx.y;
//
//  Nothing about the hardware is two-dimensional. This is purely a
//  convenience for data that is: an image, a matrix, a grid of cells. The
//  mapping to memory is still yours to write, and for a row-major matrix it
//  is `row * width + col`. (Cautaerts & Ghorbanfekr, GPU-Accelerated
//  Computing with Python 3 and CUDA, ch. 3 "Dealing with a mismatched grid
//  and problem size", uses a 16 x 16 block for an image for the same reason:
//  256 threads, a multiple of 32, and a shape that matches the data.)
//
//  Put the X coordinate along the row -- the fastest-varying index in memory.
//  Consecutive threads (consecutive threadIdx.x) then touch consecutive
//  addresses, which is what the memory system is built for. 15.01 measures
//  what happens if you do it the other way round.
//
//  The grid must be a whole number of blocks in each direction, and the data
//  is almost never a multiple of 16 in both. So the grid is rounded UP, and
//  the block at the end of each row and each column has threads whose (row,
//  col) lies outside the matrix. Without a guard they compute an index past
//  the end and write there -- into whatever is next in device memory. For a
//  1000 x 777 matrix in 16 x 16 blocks that is 63 x 49 blocks, 1008 x 784
//  threads, and 13,568 threads with nothing to do.
//
//  This kind of overrun is the classic CUDA bug: it usually does not crash,
//  because the allocation granularity is coarse and the bytes after the
//  matrix are often yours anyway. It corrupts something else, later. The
//  tests here put a guarded region after the matrix so the overrun is
//  visible; in real code, `compute-sanitizer --tool memcheck` (17.04) is how
//  you find it.
//
//  TASK
//    Make `grid_for` round up, and add the bounds guard to the kernel.
//
//  RUN IT
//    ./mcpp test 14_04
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

// 16 x 16 = 256 threads: a multiple of the warp size, and small enough in
// each dimension that a partial block at the edge wastes little.
constexpr unsigned kTile = 16;

// Thread x runs along the row (the column index), because consecutive
// threads then touch consecutive addresses. 15.01 is about why that matters.
__global__ void scale_kernel(const float* in, float* out, unsigned width, unsigned height,
                             float factor) {
  const unsigned col = blockIdx.x * blockDim.x + threadIdx.x;
  const unsigned row = blockIdx.y * blockDim.y + threadIdx.y;
  // TODO: the grid is rounded up to whole blocks, so the last block in each
  // direction has threads whose row or column lies outside the matrix. Guard
  // against them, or they read and write past the end.
  const std::size_t index = static_cast<std::size_t>(row) * width + col;
  out[index] = in[index] * factor;
}

// TODO: round up in both directions. 1000 columns need 63 blocks of 16, not 62.
dim3 grid_for(unsigned width, unsigned height) {
  return dim3(width / kTile, height / kTile);
}

// Scales a row-major `height` x `width` matrix on the device. The spans may be
// longer than width*height: the extra elements belong to the caller, and the
// kernel must not touch them.
void scale_matrix(std::span<const float> in, std::span<float> out, unsigned width,
                  unsigned height, float factor) {
  if (in.size() != out.size() || in.size() < static_cast<std::size_t>(width) * height) {
    throw std::invalid_argument("scale_matrix: bad sizes");
  }
  DeviceBuffer<float> d_in(in.size());
  DeviceBuffer<float> d_out(out.size());
  d_in.upload(in);
  d_out.upload(out); // the caller's sentinels travel with it

  const dim3 block(kTile, kTile);
  scale_kernel<<<grid_for(width, height), block>>>(d_in.data(), d_out.data(), width,
                                                   height, factor);
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());
  d_out.download(out);
}

namespace {

constexpr float kSentinel = -12345.0F;

// A matrix followed by a guard region. In `in` the guard is zero, so anything
// read from it and scaled comes out as 0; in `out` the guard is a sentinel, so
// any write into it is visible.
struct Fixture {
  unsigned width;
  unsigned height;
  std::size_t guard;
  std::vector<float> in;
  std::vector<float> out;

  Fixture(unsigned w, unsigned h, std::size_t g)
      : width(w), height(h), guard(g), in(static_cast<std::size_t>(w) * h + g, 0.0F),
        out(static_cast<std::size_t>(w) * h + g, kSentinel) {
    for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
      in[i] = static_cast<float>(i % 97) + 1.0F;
      out[i] = kSentinel;
    }
  }

  std::size_t elements() const {
    return static_cast<std::size_t>(width) * height;
  }
  bool matrix_correct(float factor) const {
    for (std::size_t i = 0; i < elements(); ++i) {
      if (out[i] != in[i] * factor) {
        return false;
      }
    }
    return true;
  }
  bool guard_intact() const {
    for (std::size_t i = elements(); i < out.size(); ++i) {
      if (out[i] != kSentinel) {
        return false;
      }
    }
    return true;
  }
};

} // namespace

TEST_CASE("grid_for rounds up in both directions") {
  const dim3 exact = grid_for(64, 32);
  CHECK(exact.x == 4);
  CHECK(exact.y == 2);
  CHECK(exact.z == 1);

  const dim3 ragged = grid_for(1000, 777);
  CHECK(ragged.x == 63); // 62 full blocks and one with 8 columns
  CHECK(ragged.y == 49); // 48 full blocks and one with 9 rows
}

TEST_CASE("a matrix whose size is a multiple of the block is scaled") {
  Fixture f(64, 32, 0);
  scale_matrix(f.in, f.out, f.width, f.height, 2.0F);
  CHECK(f.matrix_correct(2.0F));
}

TEST_CASE("a ragged matrix is scaled completely") {
  Fixture f(1000, 777, 8192);
  scale_matrix(f.in, f.out, f.width, f.height, 3.0F);
  CHECK(f.matrix_correct(3.0F));
  CHECK(f.out[f.elements() - 1] == f.in[f.elements() - 1] * 3.0F);
}

TEST_CASE("threads outside the matrix write nothing") {
  // 63 x 49 blocks cover 1008 x 784 threads for a 1000 x 777 matrix. The
  // extra 8 columns and 7 rows must not write anywhere -- and the guard
  // region right after the matrix is where they would land.
  Fixture f(1000, 777, 8192);
  scale_matrix(f.in, f.out, f.width, f.height, 1.5F);
  CHECK(f.guard_intact());
}

TEST_CASE("a single-element matrix works") {
  Fixture f(1, 1, 512);
  scale_matrix(f.in, f.out, 1, 1, 10.0F);
  CHECK(f.out[0] == 10.0F);
  CHECK(f.guard_intact());
}
