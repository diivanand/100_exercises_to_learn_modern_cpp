// Solution -- 16.01 Thrust
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

#include <cuda_runtime.h>
#include <thrust/copy.h>
#include <thrust/device_vector.h>
#include <thrust/functional.h>
#include <thrust/reduce.h>
#include <thrust/sort.h>
#include <thrust/transform.h>
#include <thrust/transform_reduce.h>

// Sum of squares, entirely on the device. transform_reduce fuses the square
// and the sum into one pass over memory; a transform into a temporary vector
// followed by a reduce would read and write the data twice.
float sum_of_squares(const thrust::device_vector<float>& values) {
  return thrust::transform_reduce(
      values.begin(), values.end(), [] __host__ __device__(float x) { return x * x; },
      0.0F, thrust::plus<float>{});
}

// Standardise in place: subtract the mean, divide by the standard deviation.
// Two reductions and one transform: three kernel launches and no element ever
// crosses the PCIe bus.
void standardise(thrust::device_vector<float>& values) {
  const auto n = static_cast<float>(values.size());
  const float mean =
      thrust::reduce(values.begin(), values.end(), 0.0F, thrust::plus<float>{}) / n;
  const float variance =
      thrust::transform_reduce(
          values.begin(), values.end(),
          [mean] __host__ __device__(float x) { return (x - mean) * (x - mean); }, 0.0F,
          thrust::plus<float>{}) /
      n;
  const float inv_std = 1.0F / std::sqrt(variance);
  thrust::transform(
      values.begin(), values.end(), values.begin(),
      [mean, inv_std] __host__ __device__(float x) { return (x - mean) * inv_std; });
}

// Returns `values` ordered by ascending `keys`. sort_by_key moves the values
// along with the keys; sorting the keys alone would lose the correspondence.
std::vector<int> ordered_by_key(const std::vector<int>& keys,
                                const std::vector<int>& values) {
  thrust::device_vector<int> d_keys(keys.begin(), keys.end());
  thrust::device_vector<int> d_values(values.begin(), values.end());
  thrust::sort_by_key(d_keys.begin(), d_keys.end(), d_values.begin());
  std::vector<int> result(values.size());
  thrust::copy(d_values.begin(), d_values.end(), result.begin());
  return result;
}

// Interoperation with a hand-written kernel: raw_pointer_cast hands the
// device_vector's storage to code that knows nothing about Thrust.
__global__ void scale_kernel(float* data, std::size_t n, float factor) {
  const std::size_t i = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
  if (i < n) {
    data[i] *= factor;
  }
}

void scale(thrust::device_vector<float>& values, float factor) {
  constexpr unsigned kBlock = 256;
  const auto n = values.size();
  const auto grid = static_cast<unsigned>((n + kBlock - 1) / kBlock);
  scale_kernel<<<grid, kBlock>>>(thrust::raw_pointer_cast(values.data()), n, factor);
  if (const cudaError_t status = cudaGetLastError(); status != cudaSuccess) {
    throw std::runtime_error(cudaGetErrorString(status));
  }
  if (const cudaError_t status = cudaDeviceSynchronize(); status != cudaSuccess) {
    throw std::runtime_error(cudaGetErrorString(status));
  }
}

// --- tests -------------------------------------------------------------------

std::vector<float> random_floats(std::size_t n, unsigned seed) {
  std::mt19937 engine(seed);
  std::normal_distribution<float> normal(3.0F, 2.0F);
  std::vector<float> out(n);
  for (float& x : out) {
    x = normal(engine);
  }
  return out;
}

// The idiomatic version, so the test can say "no slower than this, within a
// generous margin" without hard-coding a time.
float reference_sum_of_squares(const thrust::device_vector<float>& values) {
  return thrust::transform_reduce(
      values.begin(), values.end(), [] __host__ __device__(float x) { return x * x; },
      0.0F, thrust::plus<float>{});
}

template <typename F>
double best_of_ms(int repetitions, F&& work) {
  double best = 1e300;
  work(); // warm-up: the first call of a Thrust algorithm loads its kernels
  for (int i = 0; i < repetitions; ++i) {
    const auto start = std::chrono::steady_clock::now();
    work();
    const auto stop = std::chrono::steady_clock::now();
    best =
        std::min(best, std::chrono::duration<double, std::milli>(stop - start).count());
  }
  return best;
}

TEST_CASE("sum_of_squares") {
  const std::vector<float> host{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  const thrust::device_vector<float> device(host.begin(), host.end());
  CHECK(sum_of_squares(device) == doctest::Approx(385.0F));
}

TEST_CASE("standardise gives mean 0 and standard deviation 1") {
  const std::vector<float> host = random_floats(1U << 15U, 42);
  thrust::device_vector<float> device(host.begin(), host.end());
  standardise(device);

  std::vector<float> back(host.size());
  thrust::copy(device.begin(), device.end(), back.begin());
  const double n = static_cast<double>(back.size());
  const double mean = std::accumulate(back.begin(), back.end(), 0.0) / n;
  double variance = 0.0;
  for (const float x : back) {
    variance += (x - mean) * (x - mean);
  }
  variance /= n;
  CHECK(std::abs(mean) < 1e-3);
  CHECK(std::abs(std::sqrt(variance) - 1.0) < 1e-3);
  // The largest input must still be the largest output: a standardisation is
  // monotonic, and a bug that sorted or shuffled would not be.
  const auto max_in = std::max_element(host.begin(), host.end()) - host.begin();
  const auto max_out = std::max_element(back.begin(), back.end()) - back.begin();
  CHECK(max_in == max_out);
}

TEST_CASE("ordered_by_key moves the values with their keys") {
  const std::vector<int> keys{30, 10, 20, 40};
  const std::vector<int> values{3, 1, 2, 4};
  CHECK(ordered_by_key(keys, values) == std::vector<int>{1, 2, 3, 4});

  // A larger, shuffled case: value == 2 * key, so after sorting by key the
  // values must be 0, 2, 4, ...
  constexpr int kCount = 100'000;
  std::vector<int> big_keys(kCount);
  std::iota(big_keys.begin(), big_keys.end(), 0);
  std::shuffle(big_keys.begin(), big_keys.end(), std::mt19937{7});
  std::vector<int> big_values(kCount);
  for (int i = 0; i < kCount; ++i) {
    big_values[static_cast<std::size_t>(i)] = 2 * big_keys[static_cast<std::size_t>(i)];
  }
  const std::vector<int> sorted = ordered_by_key(big_keys, big_values);
  bool ok = true;
  for (int i = 0; i < kCount; ++i) {
    ok = ok && sorted[static_cast<std::size_t>(i)] == 2 * i;
  }
  CHECK(ok);
}

TEST_CASE("a hand-written kernel can work on a device_vector") {
  const std::vector<float> host{1, 2, 3};
  thrust::device_vector<float> device(host.begin(), host.end());
  scale(device, 10.0F);
  std::vector<float> back(3);
  thrust::copy(device.begin(), device.end(), back.begin());
  CHECK(back == std::vector<float>{10, 20, 30});
}

TEST_CASE("the work stays on the device") {
  // Reading device_vector elements one at a time from the host is correct and
  // catastrophically slow: every `v[i]` is a synchronous cudaMemcpy. The
  // margin here is 20x against the idiomatic version; the element-wise loop
  // is thousands of times slower, so this is not a close call.
  const std::vector<float> host = random_floats(1U << 15U, 1);
  thrust::device_vector<float> device(host.begin(), host.end());

  const double reference_ms =
      best_of_ms(3, [&] { (void)reference_sum_of_squares(device); });
  const double yours_ms = best_of_ms(3, [&] {
    (void)sum_of_squares(device);
    standardise(device);
  });
  MESSAGE("reference " << reference_ms << " ms, yours " << yours_ms << " ms");
  // Three algorithms against one, so allow 3x for the extra launches on top
  // of the 20x margin.
  CHECK(yours_ms < 60.0 * reference_ms + 0.5);
}
