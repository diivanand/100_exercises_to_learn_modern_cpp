// Solution -- 15.05 Unified memory and prefetching
#include <doctest/doctest.h>

#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

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

// A DeviceBuffer (14.03) whose memory the host can also touch. Same rule of
// five; the one difference is cudaMallocManaged, and that operator[] is legal
// on the host.
template <typename T>
class ManagedArray {
public:
  explicit ManagedArray(std::size_t count) : count_(count) {
    void* raw = nullptr;
    CUDA_CHECK(cudaMallocManaged(&raw, count_ * sizeof(T)));
    data_ = static_cast<T*>(raw);
  }
  ~ManagedArray() {
    cudaFree(data_);
  }
  ManagedArray(const ManagedArray&) = delete;
  ManagedArray& operator=(const ManagedArray&) = delete;
  ManagedArray(ManagedArray&& other) noexcept
      : data_(std::exchange(other.data_, nullptr)),
        count_(std::exchange(other.count_, 0)) {}
  ManagedArray& operator=(ManagedArray&& other) noexcept {
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
  T& operator[](std::size_t i) {
    return data_[i];
  }
  const T& operator[](std::size_t i) const {
    return data_[i];
  }

private:
  T* data_ = nullptr;
  std::size_t count_ = 0;
};

class Stream {
public:
  Stream() {
    CUDA_CHECK(cudaStreamCreate(&stream_));
  }
  ~Stream() {
    cudaStreamDestroy(stream_);
  }
  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;
  cudaStream_t get() const noexcept {
    return stream_;
  }

private:
  cudaStream_t stream_{};
};

// Where memory should live, as the runtime spells it: a GPU by ordinal, or
// "the host". CUDA 12.2 introduced cudaMemLocation for these calls and CUDA
// 13 made it the only form, so the two wrappers below keep one spelling
// working on both toolkits.
cudaMemLocation device_location(int device) {
  cudaMemLocation location{};
  location.type = cudaMemLocationTypeDevice;
  location.id = device;
  return location;
}

cudaMemLocation host_location() {
  cudaMemLocation location{};
  location.type = cudaMemLocationTypeHost;
  location.id = 0;
  return location;
}

void prefetch(const void* ptr, std::size_t bytes, cudaMemLocation where,
              cudaStream_t stream) {
#if CUDART_VERSION >= 13000
  CUDA_CHECK(cudaMemPrefetchAsync(ptr, bytes, where, 0, stream));
#else
  CUDA_CHECK(cudaMemPrefetchAsync_v2(ptr, bytes, where, 0, stream));
#endif
}

void advise(const void* ptr, std::size_t bytes, cudaMemoryAdvise advice,
            cudaMemLocation where) {
#if CUDART_VERSION >= 13000
  CUDA_CHECK(cudaMemAdvise(ptr, bytes, advice, where));
#else
  CUDA_CHECK(cudaMemAdvise_v2(ptr, bytes, advice, where));
#endif
}

constexpr unsigned kBlock = 256;

__global__ void scale_kernel(float* values, std::size_t n, float factor) {
  const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  if (i < n) {
    values[i] *= factor;
  }
}

// Scales a managed array on the GPU and hands it back to the host, moving the
// pages in bulk in both directions rather than one fault at a time.
void scale_in_place(ManagedArray<float>& values, float factor, cudaStream_t stream) {
  int device = 0;
  CUDA_CHECK(cudaGetDevice(&device));

  // Policy first: this range's home is the GPU. Without it, a later host
  // access would migrate the pages back for good, and the next kernel would
  // fault them in all over again.
  advise(values.data(), values.bytes(), cudaMemAdviseSetPreferredLocation,
         device_location(device));

  // Mechanism: move every page now, on the stream, before the kernel needs
  // them. A prefetch is a DMA transfer at bus speed; a page fault is a
  // round-trip through the driver per page.
  prefetch(values.data(), values.bytes(), device_location(device), stream);

  const auto blocks = static_cast<unsigned>((values.size() + kBlock - 1) / kBlock);
  scale_kernel<<<blocks, kBlock, 0, stream>>>(values.data(), values.size(), factor);
  CUDA_CHECK(cudaGetLastError());

  // And back, so that the host's reads below do not fault the pages over one
  // at a time either. Stream order guarantees this waits for the kernel.
  prefetch(values.data(), values.bytes(), host_location(), stream);

  // The host must not touch managed memory that a kernel on the stream may
  // still be using: on a PCIe GPU without HMM that is a segmentation fault,
  // not a stale read.
  CUDA_CHECK(cudaStreamSynchronize(stream));
}

// --- tests -------------------------------------------------------------------

cudaMemLocationType last_prefetch_location_type(const ManagedArray<float>& values) {
  cudaMemLocationType type = cudaMemLocationTypeInvalid;
  CUDA_CHECK(cudaMemRangeGetAttribute(&type, sizeof(type),
                                      cudaMemRangeAttributeLastPrefetchLocationType,
                                      values.data(), values.bytes()));
  return type;
}

cudaMemLocationType preferred_location_type(const ManagedArray<float>& values) {
  cudaMemLocationType type = cudaMemLocationTypeInvalid;
  CUDA_CHECK(cudaMemRangeGetAttribute(&type, sizeof(type),
                                      cudaMemRangeAttributePreferredLocationType,
                                      values.data(), values.bytes()));
  return type;
}

int preferred_location_id(const ManagedArray<float>& values) {
  int id = -1;
  CUDA_CHECK(cudaMemRangeGetAttribute(&id, sizeof(id),
                                      cudaMemRangeAttributePreferredLocationId,
                                      values.data(), values.bytes()));
  return id;
}

TEST_CASE("the host sees the scaled values") {
  const std::size_t n = std::size_t{1} << 24; // 64 MB
  ManagedArray<float> values(n);
  std::iota(values.data(), values.data() + n, 0.0F);

  Stream stream;
  scale_in_place(values, 0.5F, stream.get());

  for (std::size_t i = 0; i < n; ++i) {
    if (values[i] != static_cast<float>(i) * 0.5F) {
      CAPTURE(i);
      REQUIRE(values[i] == static_cast<float>(i) * 0.5F);
    }
  }
}

TEST_CASE("the range was prefetched back to the host, not faulted back") {
  const std::size_t n = std::size_t{1} << 20;
  ManagedArray<float> values(n);
  std::iota(values.data(), values.data() + n, 0.0F);

  Stream stream;
  scale_in_place(values, 2.0F, stream.get());

  CHECK(last_prefetch_location_type(values) == cudaMemLocationTypeHost);
}

TEST_CASE("the range's preferred location is the GPU") {
  const std::size_t n = std::size_t{1} << 20;
  ManagedArray<float> values(n);
  std::iota(values.data(), values.data() + n, 0.0F);

  Stream stream;
  scale_in_place(values, 2.0F, stream.get());

  int device = 0;
  CUDA_CHECK(cudaGetDevice(&device));
  CHECK(preferred_location_type(values) == cudaMemLocationTypeDevice);
  CHECK(preferred_location_id(values) == device);
}

TEST_CASE("managed memory is addressable from both sides") {
  ManagedArray<float> values(4);
  cudaPointerAttributes attributes{};
  CUDA_CHECK(cudaPointerGetAttributes(&attributes, values.data()));
  CHECK(attributes.type == cudaMemoryTypeManaged);
  static_assert(!std::is_copy_constructible_v<ManagedArray<float>>);
  static_assert(std::is_nothrow_move_constructible_v<ManagedArray<float>>);
}
