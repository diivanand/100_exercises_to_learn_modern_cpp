// =============================================================================
//  14.03 -- Device memory and RAII
// =============================================================================
//
//  14.01 called cudaMalloc three times and cudaFree three times, and if any
//  copy in between had thrown, the frees would never have run. That is the
//  bug 03.06 was about, with a different allocator: a resource acquired in
//  one place and released in another, joined by nothing but hope.
//
//  The answer is the same too. Put the resource in a class whose destructor
//  releases it (RAII; Core Guidelines R.1), and let the language run the
//  destructor on every path out of the scope, exceptions included. For device
//  memory that class is
//
//      template <typename T>
//      class DeviceBuffer;     // owns count * sizeof(T) bytes on the device
//
//  and writing it is an exercise in the rule of five (03.05):
//
//   * DESTRUCTOR: cudaFree. It cannot report failure -- a destructor that
//     throws is a terminated program -- and the runtime treats cudaFree of a
//     null pointer as a no-op, exactly like delete.
//
//   * COPY: deleted. A device buffer is not a value. A "copy" would either
//     be a fresh allocation plus a device-to-device transfer, which nobody
//     should get by accident, or two owners of one pointer, which is a
//     double free. Deleting both copy operations makes the question go away.
//     (03.05: declaring the destructor suppresses the implicit moves; you
//     have to write them back. Declaring nothing at all gives you copies
//     that are shallow, and a "move" that is a shallow copy.)
//
//   * MOVE: steal the pointer and the size, leave the source empty. Mark it
//     noexcept (02.04): std::vector will only move its elements on growth if
//     the move cannot throw, and otherwise falls back to a copy, which here
//     does not exist.
//
//  The buffer also knows its size, so the copies in and out can be checked,
//  and takes std::span (06.05) so a std::vector, a std::array or a pinned
//  buffer (15.06) all work without overloads.
//
//  cudaMemGetInfo reports free and total device memory, and the tests use it
//  to prove that memory really comes back: sixteen 64 MB buffers that are
//  never freed are a gigabyte gone. (PMPP §3.4 shows the raw calls; Motta
//  ch. 10 "Moving sequential code to the GPU" makes the same argument for
//  wrapping them in C++.)
//
//  TASK
//    Give DeviceBuffer a destructor, deleted copies, and noexcept moves.
//
//  RUN IT
//    ./mcpp test 14_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
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

// Owns one device allocation.
template <typename T>
class DeviceBuffer {
public:
  DeviceBuffer() = default;

  explicit DeviceBuffer(std::size_t count) : count_(count) {
    if (count_ > 0) {
      CUDA_CHECK(cudaMalloc(&data_, count_ * sizeof(T)));
    }
  }

  // TODO: the rule of five.
  //   - a destructor that calls cudaFree(data_);
  //   - copy constructor and copy assignment `= delete`;
  //   - a move constructor and move assignment, both noexcept, that take the
  //     other buffer's pointer and size and leave it holding nullptr and 0
  //     (std::exchange does this in one expression). Move assignment must
  //     free what this buffer held first, and cope with self-assignment.

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

  // std::span (06.05) says "these elements" without caring whether they live
  // in a vector, an array or a pinned buffer, and carries the length so the
  // copy cannot overrun either side.
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

// Free device memory in bytes, as the driver sees it.
std::size_t free_device_bytes() {
  std::size_t free_bytes = 0;
  std::size_t total_bytes = 0;
  CUDA_CHECK(cudaMemGetInfo(&free_bytes, &total_bytes));
  return free_bytes;
}

TEST_CASE("a DeviceBuffer is not copyable and moves without throwing") {
  CHECK_FALSE(std::is_copy_constructible_v<DeviceBuffer<float>>);
  CHECK_FALSE(std::is_copy_assignable_v<DeviceBuffer<float>>);
  CHECK(std::is_nothrow_move_constructible_v<DeviceBuffer<float>>);
  CHECK(std::is_nothrow_move_assignable_v<DeviceBuffer<float>>);
}

TEST_CASE("memory is returned when the buffer goes out of scope") {
  // The first allocation also creates the CUDA context, which reserves memory
  // of its own. Do that before taking the baseline.
  {
    [[maybe_unused]] const DeviceBuffer<float> warm_up(1024);
  }
  const std::size_t before = free_device_bytes();

  constexpr std::size_t kCount = 16U << 20; // 64 MB of floats
  for (int i = 0; i < 16; ++i) {
    const DeviceBuffer<float> buffer(kCount);
    CHECK(buffer.size() == kCount);
    CHECK(buffer.bytes() == kCount * sizeof(float));
    CHECK(buffer.data() != nullptr);
  }

  // Sixteen leaked buffers would be a gigabyte. Allow the driver some slack.
  const std::size_t after = free_device_bytes();
  const std::size_t slack = 64U << 20;
  CHECK(after + slack >= before);
}

TEST_CASE("moving transfers ownership and leaves the source empty") {
  DeviceBuffer<int> source(100);
  const int* raw = source.data();

  DeviceBuffer<int> target(std::move(source));
  CHECK(target.data() == raw);
  CHECK(target.size() == 100);
  CHECK(source.data() == nullptr); // NOLINT(bugprone-use-after-move) -- the point
  CHECK(source.size() == 0);

  DeviceBuffer<int> other(5);
  other = std::move(target);
  CHECK(other.data() == raw);
  CHECK(other.size() == 100);
  CHECK(target.data() == nullptr); // NOLINT(bugprone-use-after-move)
}

TEST_CASE("upload and download round-trip through any contiguous host range") {
  const std::vector<float> from_vector = {1.0F, 2.0F, 3.0F, 4.0F};
  DeviceBuffer<float> buffer(from_vector.size());
  buffer.upload(from_vector);

  std::array<float, 4> into_array{};
  buffer.download(into_array);
  CHECK(into_array == std::array<float, 4>{1.0F, 2.0F, 3.0F, 4.0F});

  std::vector<float> wrong_size(3);
  CHECK_THROWS_AS(buffer.download(wrong_size), std::invalid_argument);
}

TEST_CASE("an empty buffer owns nothing and is harmless") {
  const DeviceBuffer<double> empty;
  CHECK(empty.data() == nullptr);
  CHECK(empty.size() == 0);
  const DeviceBuffer<double> zero(0);
  CHECK(zero.data() == nullptr);
}
