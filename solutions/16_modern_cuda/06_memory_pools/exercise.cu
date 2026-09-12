// Solution -- 16.06 Memory pools
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

private:
  T* data_ = nullptr;
  std::size_t count_ = 0;
};

// A stream with a lifetime (03.06).
class Stream {
public:
  Stream() {
    CUDA_CHECK(cudaStreamCreate(&handle_));
  }
  ~Stream() {
    cudaStreamDestroy(handle_);
  }
  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;

  cudaStream_t get() const noexcept {
    return handle_;
  }
  void wait() const {
    CUDA_CHECK(cudaStreamSynchronize(handle_));
  }

private:
  cudaStream_t handle_ = nullptr;
};

cudaMemPool_t default_pool() {
  int device = 0;
  CUDA_CHECK(cudaGetDevice(&device));
  cudaMemPool_t pool = nullptr;
  CUDA_CHECK(cudaDeviceGetDefaultMemPool(&pool, device));
  return pool;
}

// The release threshold is the amount of memory the pool keeps hold of when
// the stream synchronises. The default is 0: everything freed goes back to
// the driver at the next sync, and the next cudaMallocAsync has to get it
// again. "Keep everything" is the usual production setting.
void configure_pool() {
  cuuint64_t threshold = UINT64_MAX;
  CUDA_CHECK(cudaMemPoolSetAttribute(default_pool(), cudaMemPoolAttrReleaseThreshold,
                                     &threshold));
}

cuuint64_t pool_attribute(cudaMemPoolAttr attribute) {
  cuuint64_t value = 0;
  CUDA_CHECK(cudaMemPoolGetAttribute(default_pool(), attribute, &value));
  return value;
}

void reset_pool_high_watermarks() {
  cuuint64_t zero = 0;
  CUDA_CHECK(cudaMemPoolSetAttribute(default_pool(), cudaMemPoolAttrUsedMemHigh, &zero));
  CUDA_CHECK(
      cudaMemPoolSetAttribute(default_pool(), cudaMemPoolAttrReservedMemHigh, &zero));
}

constexpr unsigned kBlock = 256;

__global__ void double_kernel(const float* in, float* scratch, std::size_t n) {
  const std::size_t i = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
  if (i < n) {
    scratch[i] = 2.0F * in[i];
  }
}

__global__ void accumulate_kernel(const float* scratch, float* out, std::size_t n) {
  const std::size_t i = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
  if (i < n) {
    out[i] += scratch[i];
  }
}

// out += 2 * in, `iterations` times, with a fresh scratch buffer each time.
// cudaMallocAsync and cudaFreeAsync are stream-ordered: the allocation is
// valid from the point in the stream where it was requested, and the memory
// goes back to the pool when the stream reaches the free. No synchronisation,
// and after the first iteration no new memory either -- the pool hands the
// same bytes straight back.
void accumulate_doubled(const DeviceBuffer<float>& in, DeviceBuffer<float>& out,
                        int iterations, cudaStream_t stream) {
  const std::size_t n = in.size();
  const auto grid = static_cast<unsigned>((n + kBlock - 1) / kBlock);
  for (int i = 0; i < iterations; ++i) {
    void* raw = nullptr;
    CUDA_CHECK(cudaMallocAsync(&raw, n * sizeof(float), stream));
    auto* scratch = static_cast<float*>(raw);

    double_kernel<<<grid, kBlock, 0, stream>>>(in.data(), scratch, n);
    CUDA_CHECK(cudaGetLastError());
    accumulate_kernel<<<grid, kBlock, 0, stream>>>(scratch, out.data(), n);
    CUDA_CHECK(cudaGetLastError());

    CUDA_CHECK(cudaFreeAsync(raw, stream));
  }
}

// --- tests -------------------------------------------------------------------

constexpr std::size_t kCount = std::size_t{1} << 22U; // 16 MB of floats
constexpr int kIterations = 50;

TEST_CASE("configure_pool keeps freed memory in the pool") {
  configure_pool();
  CHECK(pool_attribute(cudaMemPoolAttrReleaseThreshold) == UINT64_MAX);
}

TEST_CASE("accumulate_doubled computes iterations * 2 * in") {
  configure_pool();
  std::vector<float> host(kCount);
  for (std::size_t i = 0; i < kCount; ++i) {
    host[i] = static_cast<float>(i % 13);
  }
  DeviceBuffer<float> in(kCount);
  in.upload(host);
  DeviceBuffer<float> out(kCount);
  CUDA_CHECK(cudaMemset(out.data(), 0, kCount * sizeof(float)));

  const Stream stream;
  accumulate_doubled(in, out, kIterations, stream.get());
  stream.wait();

  std::vector<float> result(kCount);
  out.download(result);
  bool ok = true;
  for (std::size_t i = 0; i < kCount; ++i) {
    ok = ok && result[i] == static_cast<float>(2 * kIterations) * host[i];
  }
  CHECK(ok);
}

TEST_CASE("the scratch buffers come from the pool") {
  // cudaMalloc bypasses the pool entirely, so its high-water mark would stay
  // at zero. A stream-ordered allocation of the scratch buffer shows up here.
  configure_pool();
  reset_pool_high_watermarks();

  DeviceBuffer<float> in(kCount);
  DeviceBuffer<float> out(kCount);
  CUDA_CHECK(cudaMemset(in.data(), 0, kCount * sizeof(float)));
  CUDA_CHECK(cudaMemset(out.data(), 0, kCount * sizeof(float)));
  const Stream stream;
  accumulate_doubled(in, out, 3, stream.get());
  stream.wait();

  CHECK(pool_attribute(cudaMemPoolAttrUsedMemHigh) >= kCount * sizeof(float));
}

TEST_CASE("after a sync the pool still holds the memory it was given") {
  // With the default threshold of zero this would read 0: the pool returns
  // every unused byte to the driver at the synchronisation. With the
  // threshold raised it keeps them for the next cudaMallocAsync.
  configure_pool();
  DeviceBuffer<float> in(kCount);
  DeviceBuffer<float> out(kCount);
  CUDA_CHECK(cudaMemset(in.data(), 0, kCount * sizeof(float)));
  CUDA_CHECK(cudaMemset(out.data(), 0, kCount * sizeof(float)));
  const Stream stream;
  accumulate_doubled(in, out, 3, stream.get());
  stream.wait();
  CUDA_CHECK(cudaDeviceSynchronize());

  CHECK(pool_attribute(cudaMemPoolAttrReservedMemCurrent) >= kCount * sizeof(float));
}
