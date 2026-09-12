// Solution -- 14.04 Grids, blocks and bounds
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
  // The grid is rounded up to whole blocks, so the last block in each
  // direction has threads with nothing to do. Without this guard they would
  // read and write outside the matrix.
  if (row < height && col < width) {
    const std::size_t index = static_cast<std::size_t>(row) * width + col;
    out[index] = in[index] * factor;
  }
}

// Whole blocks, rounded up, in each direction.
dim3 grid_for(unsigned width, unsigned height) {
  return dim3((width + kTile - 1) / kTile, (height + kTile - 1) / kTile);
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
