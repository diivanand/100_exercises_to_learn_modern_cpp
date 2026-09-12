// =============================================================================
//  17.01 -- Events and bandwidth
// =============================================================================
//
//  Before you can make anything faster you have to be able to measure it, and
//  on a GPU the obvious way of measuring is wrong. A kernel launch is
//  asynchronous: `kernel<<<...>>>()` returns to the host as soon as the work is
//  queued, microseconds later, while the kernel itself may run for
//  milliseconds. A host clock around the launch therefore times the launch.
//  Cautaerts ch. 4 ("CPU time versus GPU time") puts it plainly: the CPU's
//  perception of elapsed time does not represent the time the GPU spent, and
//  to measure GPU time you must rely on CUDA events or a timeline profiler.
//
//  EVENTS. A cudaEvent_t is a marker in a stream. `cudaEventRecord` queues it
//  behind whatever is already queued; it "happens" when the GPU reaches it.
//  Record one before the work and one after, `cudaEventSynchronize` the
//  second, and `cudaEventElapsedTime` gives the milliseconds between them by
//  the GPU's own clock (Motta ch. 4, "How to measure execution time on the
//  GPU"). The `GpuTimer` below wraps that in RAII, the way 03.06 taught.
//
//  WARM-UP. The first launch of a kernel loads its code onto the device, the
//  first cudaMalloc creates the context, and the clocks may still be
//  ramping. Motta's advice is to run the code a few times untimed first. The
//  `best_of` helper you will write does one untimed call, then takes the
//  MINIMUM of N timed runs: the minimum is the least-disturbed run, and the
//  number Nsight will show you. A mean smuggles every interruption back in.
//
//  BANDWIDTH. A copy kernel moves 2 * n * sizeof(float) bytes: each element is
//  read once and written once. Divide by the time and you have achieved
//  bandwidth in GB/s. Compare it with the peak the memory system could
//  deliver -- two transfers per clock (double data rate) times the bus width
//  -- and you know whether the kernel is near the roofline (PMPP §6.2 works
//  through this arithmetic; Cautaerts ch. 4, "Memory- versus compute-bound
//  kernels", explains why a copy is the definition of memory-bound). On an
//  RTX 4090 the peak is about 1008 GB/s and a plain copy reaches 85-90% of it.
//
//  Since the books: cudaDeviceProp::memoryClockRate and friends were removed
//  in CUDA 13. `cudaDeviceGetAttribute` with cudaDevAttrMemoryClockRate (kHz)
//  and cudaDevAttrGlobalMemoryBusWidth (bits) is the spelling that works on
//  every current toolkit, and is what `peak_bandwidth_gbs` uses.
//
//  TASK
//    `best_of` times with a host clock and no synchronisation, so the copy
//    appears to run faster than the memory bus allows. Make it warm up, use
//    `GpuTimer`, and return the best of `repetitions` runs.
//
//  RUN IT
//    ./mcpp test 17_01
//
// =============================================================================
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
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

// --- timing ------------------------------------------------------------------

// Two events bracket work on a stream. The elapsed time between them is taken
// from the GPU's own clock, after the work has actually finished, so it does
// not matter that the host returned from the launch microseconds earlier.
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

  void start(cudaStream_t stream = nullptr) {
    CUDA_CHECK(cudaEventRecord(start_, stream));
  }
  // Returns milliseconds. Blocks until the stop event has been reached, which
  // is the synchronisation the host clock was missing.
  float stop(cudaStream_t stream = nullptr) {
    CUDA_CHECK(cudaEventRecord(stop_, stream));
    CUDA_CHECK(cudaEventSynchronize(stop_));
    float ms = 0.0F;
    CUDA_CHECK(cudaEventElapsedTime(&ms, start_, stop_));
    return ms;
  }

private:
  cudaEvent_t start_{};
  cudaEvent_t stop_{};
};

// TODO: this is the wrong clock in the wrong place. The launch inside `work`
// returns immediately, so steady_clock measures how long it takes to *queue*
// a kernel -- a few microseconds -- regardless of how long the kernel runs.
// Nothing here waits for the GPU, there is no warm-up, and the mean mixes the
// first (slow, code-loading) run in with the rest.
//
// Use GpuTimer: one untimed call, then `timer.start(); work(); timer.stop()`
// for each repetition, keeping the minimum.
template <typename F>
float best_of(int repetitions, F&& work) {
  float total_ms = 0.0F;
  for (int i = 0; i < repetitions; ++i) {
    const auto begin = std::chrono::steady_clock::now();
    work();
    const auto end = std::chrono::steady_clock::now();
    total_ms += std::chrono::duration<float, std::milli>(end - begin).count();
  }
  return total_ms / static_cast<float>(repetitions);
}

// --- the kernels -------------------------------------------------------------

constexpr unsigned kBlock = 256;

__global__ void copy_kernel(const float* in, float* out, std::size_t n) {
  const std::size_t stride = static_cast<std::size_t>(gridDim.x) * blockDim.x;
  for (std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
       i < n; i += stride) {
    out[i] = in[i];
  }
}

// Spins for a known number of SM clock cycles. Used only to show that a host
// clock around a launch measures the launch, not the kernel.
__global__ void spin_kernel(long long cycles, int* sink) {
  const long long begin = clock64();
  while (clock64() - begin < cycles) {
  }
  if (threadIdx.x == 0) {
    *sink = 1;
  }
}

// --- what you measure ----------------------------------------------------------

struct Bandwidth {
  float milliseconds;
  double gigabytes_per_second;
};

// The bandwidth the memory system could deliver if every byte moved were
// useful: two transfers per clock (GDDR is double data rate) times the bus
// width. The attribute API replaces the cudaDeviceProp fields, which CUDA 13
// removed; the clock comes back in kHz and the bus width in bits.
double peak_bandwidth_gbs(int device) {
  int clock_khz = 0;
  int bus_bits = 0;
  CUDA_CHECK(cudaDeviceGetAttribute(&clock_khz, cudaDevAttrMemoryClockRate, device));
  CUDA_CHECK(cudaDeviceGetAttribute(&bus_bits, cudaDevAttrGlobalMemoryBusWidth, device));
  return 2.0 * clock_khz * 1000.0 * bus_bits / 8.0 / 1e9;
}

Bandwidth measure_copy(const DeviceBuffer<float>& in, DeviceBuffer<float>& out,
                       int repetitions) {
  const std::size_t n = in.size();
  const unsigned grid =
      static_cast<unsigned>(std::min<std::size_t>((n + kBlock - 1) / kBlock, 4096));
  const float ms = best_of(repetitions, [&] {
    copy_kernel<<<grid, kBlock>>>(in.data(), out.data(), n);
    CUDA_CHECK(cudaGetLastError());
  });
  // Every element is read once and written once.
  const double bytes = 2.0 * static_cast<double>(in.bytes());
  return Bandwidth{ms, bytes / (ms / 1e3) / 1e9};
}

// -----------------------------------------------------------------------------

TEST_CASE("the peak bandwidth of the device is plausible") {
  int device = 0;
  CUDA_CHECK(cudaGetDevice(&device));
  const double peak = peak_bandwidth_gbs(device);
  // Anything from a laptop GPU (~100 GB/s) to an HBM part (~8 TB/s).
  CHECK(peak > 100.0);
  CHECK(peak < 10000.0);
}

TEST_CASE("a host clock around a launch measures the launch, not the kernel") {
  DeviceBuffer<int> sink(1);
  // Five million SM cycles is at least 1.5 ms on any GPU clocked below 3.3 GHz.
  const long long cycles = 5'000'000;
  spin_kernel<<<1, 32>>>(cycles, sink.data());
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize()); // warm-up: the first launch loads the module

  const auto begin = std::chrono::steady_clock::now();
  spin_kernel<<<1, 32>>>(cycles, sink.data());
  CUDA_CHECK(cudaGetLastError());
  const auto returned = std::chrono::steady_clock::now();
  CUDA_CHECK(cudaDeviceSynchronize());
  const auto finished = std::chrono::steady_clock::now();

  const auto to_ms = [](auto d) {
    return std::chrono::duration<double, std::milli>(d).count();
  };
  // The launch returns long before the kernel is done...
  CHECK(to_ms(returned - begin) < 1.0);
  // ...and the work only shows up once something waits for it.
  CHECK(to_ms(finished - begin) >= 1.5);

  GpuTimer timer;
  timer.start();
  spin_kernel<<<1, 32>>>(cycles, sink.data());
  CUDA_CHECK(cudaGetLastError());
  CHECK(timer.stop() >= 1.5F);
}

TEST_CASE("a copy kernel reaches a sane fraction of peak bandwidth") {
  int device = 0;
  CUDA_CHECK(cudaGetDevice(&device));
  const double peak = peak_bandwidth_gbs(device);

  constexpr std::size_t n = std::size_t{1} << 25; // 128 MB per buffer
  std::vector<float> host(n, 1.5F);
  DeviceBuffer<float> in(n);
  DeviceBuffer<float> out(n);
  in.upload(host);

  const Bandwidth measured = measure_copy(in, out, 5);
  MESSAGE("copy: " << measured.milliseconds << " ms, " << measured.gigabytes_per_second
                   << " GB/s of a theoretical " << peak);

  // A timer that does not wait for the GPU reports a copy that beats the
  // memory bus, which is how you know it is not measuring the copy.
  CHECK(measured.gigabytes_per_second <= 1.1 * peak);
  // A straightforward copy should get well over half of peak on any modern
  // part; 30% leaves room for a small, unlucky GPU.
  CHECK(measured.gigabytes_per_second >= 0.3 * peak);

  std::vector<float> back(n);
  out.download(back);
  CHECK(back.front() == 1.5F);
  CHECK(back.back() == 1.5F);
}
