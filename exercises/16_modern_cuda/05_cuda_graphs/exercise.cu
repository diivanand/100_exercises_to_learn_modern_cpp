// =============================================================================
//  16.05 -- CUDA graphs
// =============================================================================
//
//  A kernel launch costs a few microseconds of host time before the GPU sees
//  it (Cautaerts ch. 6, "GPU work queue"). A kernel that runs for a
//  millisecond does not care. A pipeline of twenty kernels that each run for
//  five microseconds -- an inference step, a small solver iteration -- spends
//  more time being launched than running, and the GPU sits idle between
//  launches, which is exactly the pattern 17.02 will show you on a timeline.
//
//  A CUDA graph records the launches once and replays them with one call:
//
//      cudaStreamBeginCapture(stream, cudaStreamCaptureModeGlobal);
//      kernel_a<<<grid, block, 0, stream>>>(...);   // recorded, not run
//      kernel_b<<<grid, block, 0, stream>>>(...);
//      cudaStreamEndCapture(stream, &graph);
//      cudaGraphInstantiate(&exec, graph, 0);       // once: validate, allocate
//      cudaGraphLaunch(exec, stream);               // many times
//
//  Nothing runs during capture. Every operation enqueued on the capturing
//  stream becomes a node; the stream ordering (plus any events, 14.06)
//  becomes the dependency edges; and the driver can then submit the whole
//  thing at once, with the per-launch overhead paid once per graph rather
//  than once per kernel. `cudaGraphGetNodes` with a null array tells you how
//  many nodes were captured, which is the quickest sanity check there is.
//
//  The mistake everybody makes once: a launch that does not name the stream
//  goes to the legacy default stream. It is not captured. In global capture
//  mode it is an error (cudaErrorStreamCaptureImplicit) and invalidates the
//  capture; with a non-blocking stream it silently runs immediately and the
//  graph comes out empty. Either way the graph does not contain your work.
//
//  A graph records work, not results. The pointers baked into it are read
//  afresh on every launch, so changing the data behind them changes the next
//  run's output; changing the pointers themselves needs
//  `cudaGraphExecKernelNodeSetParams` or a re-capture.
//
//  Since the Pipeline owns a stream, a graph and an executable graph, it is
//  an RAII type with copying deleted (03.05, 03.06). See the CUDA C++
//  Programming Guide, "CUDA Graphs" and "Creating a Graph Using Stream
//  Capture".
//
//  TASK
//    The three launches in the Pipeline constructor are not captured. Make
//    them part of the graph.
//
//  RUN IT
//    ./mcpp test 16_05
//
// =============================================================================
#include <doctest/doctest.h>

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
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

constexpr unsigned kBlock = 256;

__global__ void scale_kernel(const float* x, float factor, float* tmp, std::size_t n) {
  const std::size_t i = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
  if (i < n) {
    tmp[i] = x[i] * factor;
  }
}

__global__ void add_kernel(const float* tmp, const float* y, float* out, std::size_t n) {
  const std::size_t i = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
  if (i < n) {
    out[i] = tmp[i] + y[i];
  }
}

__global__ void square_kernel(float* out, std::size_t n) {
  const std::size_t i = blockIdx.x * static_cast<std::size_t>(blockDim.x) + threadIdx.x;
  if (i < n) {
    out[i] = out[i] * out[i];
  }
}

// out = (x * factor + y)^2, as three kernels recorded once into a graph and
// replayed with a single cudaGraphLaunch per run.
class Pipeline {
public:
  Pipeline(const DeviceBuffer<float>& x, const DeviceBuffer<float>& y,
           DeviceBuffer<float>& out, float factor)
      : tmp_(x.size()) {
    CUDA_CHECK(cudaStreamCreate(&stream_));

    const std::size_t n = x.size();
    const auto grid = static_cast<unsigned>((n + kBlock - 1) / kBlock);

    // TODO: these three launches go to the legacy default stream, so the
    // capturing stream never sees them. Give each launch the stream
    // (`<<<grid, kBlock, 0, stream_>>>`).
    CUDA_CHECK(cudaStreamBeginCapture(stream_, cudaStreamCaptureModeGlobal));
    scale_kernel<<<grid, kBlock>>>(x.data(), factor, tmp_.data(), n);
    cudaError_t launch_status = cudaGetLastError();
    add_kernel<<<grid, kBlock>>>(tmp_.data(), y.data(), out.data(), n);
    if (launch_status == cudaSuccess) {
      launch_status = cudaGetLastError();
    }
    square_kernel<<<grid, kBlock>>>(out.data(), n);
    if (launch_status == cudaSuccess) {
      launch_status = cudaGetLastError();
    }
    // Always end the capture, even after a failed launch: a stream left in
    // capture mode poisons every later operation on the legacy stream.
    const cudaError_t end_status = cudaStreamEndCapture(stream_, &graph_);
    CUDA_CHECK(launch_status);
    CUDA_CHECK(end_status);

    // Instantiation is the expensive step (validation, resource allocation)
    // and happens once; launches of the executable graph are cheap.
    CUDA_CHECK(cudaGraphInstantiate(&exec_, graph_, 0));
  }

  ~Pipeline() {
    if (exec_ != nullptr) {
      cudaGraphExecDestroy(exec_);
    }
    if (graph_ != nullptr) {
      cudaGraphDestroy(graph_);
    }
    if (stream_ != nullptr) {
      cudaStreamDestroy(stream_);
    }
  }

  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  void run() {
    CUDA_CHECK(cudaGraphLaunch(exec_, stream_));
  }

  void wait() {
    CUDA_CHECK(cudaStreamSynchronize(stream_));
  }

  std::size_t node_count() const {
    std::size_t count = 0;
    CUDA_CHECK(cudaGraphGetNodes(graph_, nullptr, &count));
    return count;
  }

private:
  DeviceBuffer<float> tmp_;
  cudaStream_t stream_ = nullptr;
  cudaGraph_t graph_ = nullptr;
  cudaGraphExec_t exec_ = nullptr;
};

// --- tests -------------------------------------------------------------------

struct Inputs {
  std::vector<float> x;
  std::vector<float> y;
  DeviceBuffer<float> d_x;
  DeviceBuffer<float> d_y;
  DeviceBuffer<float> d_out;
};

Inputs make_inputs(std::size_t n) {
  Inputs in;
  in.x.resize(n);
  in.y.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    in.x[i] = static_cast<float>(i % 100) * 0.01F;
    in.y[i] = static_cast<float>(i % 7);
  }
  in.d_x = DeviceBuffer<float>(n);
  in.d_y = DeviceBuffer<float>(n);
  in.d_out = DeviceBuffer<float>(n);
  in.d_x.upload(in.x);
  in.d_y.upload(in.y);
  return in;
}

bool matches(const std::vector<float>& out, const std::vector<float>& x,
             const std::vector<float>& y, float factor) {
  bool ok = true;
  for (std::size_t i = 0; i < out.size(); ++i) {
    const float v = x[i] * factor + y[i];
    ok = ok && out[i] == doctest::Approx(v * v);
  }
  return ok;
}

TEST_CASE("the captured graph has one node per kernel") {
  Inputs in = make_inputs(1U << 16U);
  const Pipeline pipeline(in.d_x, in.d_y, in.d_out, 2.0F);
  CHECK(pipeline.node_count() == 3);
}

TEST_CASE("launching the graph computes the pipeline") {
  Inputs in = make_inputs((1U << 20U) + 3);
  Pipeline pipeline(in.d_x, in.d_y, in.d_out, 3.0F);
  for (int i = 0; i < 10; ++i) {
    pipeline.run();
  }
  pipeline.wait();
  std::vector<float> out(in.x.size());
  in.d_out.download(out);
  CHECK(matches(out, in.x, in.y, 3.0F));
}

TEST_CASE("each launch re-executes the kernels on the current data") {
  // A graph records WORK, not results: change the input and re-launch, and
  // the output must follow. A capture that ran the kernels eagerly instead of
  // recording them would pass the previous test and fail this one.
  Inputs in = make_inputs(1U << 16U);
  Pipeline pipeline(in.d_x, in.d_y, in.d_out, 1.0F);
  pipeline.run();
  pipeline.wait();

  std::vector<float> new_x(in.x.size(), 5.0F);
  in.d_x.upload(new_x);
  pipeline.run();
  pipeline.wait();

  std::vector<float> out(in.x.size());
  in.d_out.download(out);
  CHECK(matches(out, new_x, in.y, 1.0F));
}

TEST_CASE("a Pipeline owns its handles") {
  static_assert(!std::is_copy_constructible_v<Pipeline>);
  static_assert(!std::is_copy_assignable_v<Pipeline>);
  CHECK(true);
}
