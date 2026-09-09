// Solution -- 13.05 Capstone: concurrent ingestion
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;

struct Sample {
  Timestamp at{};
  double value = 0.0;
};

struct SeriesKey {
  std::string name;
  std::map<std::string, std::string> tags;

  friend bool operator==(const SeriesKey&, const SeriesKey&) = default;
  friend std::strong_ordering operator<=>(const SeriesKey&, const SeriesKey&) = default;
};

struct Reading {
  SeriesKey key;
  Sample sample;
};

// ---------------------------------------------------------------------------
// The store, made thread-safe.
//
// A shared_mutex, because reads (queries) greatly outnumber writes (10.03) --
// and the mutex is `mutable` so the const accessors can lock it, which is
// exactly the case `mutable` exists for (04.01).
// ---------------------------------------------------------------------------

class ConcurrentStore {
public:
  void record(const SeriesKey& key, Sample sample) {
    const std::unique_lock lock{mutex_};
    series_[key].push_back(sample);
  }

  // Recording a whole batch under ONE lock, rather than locking per sample.
  // This is the difference between a store that scales and one that does not.
  void record_batch(const std::vector<Reading>& readings) {
    const std::unique_lock lock{mutex_};
    for (const auto& reading : readings) {
      series_[reading.key].push_back(reading.sample);
    }
  }

  [[nodiscard]] std::size_t series_count() const {
    const std::shared_lock lock{mutex_};
    return series_.size();
  }

  [[nodiscard]] std::size_t sample_count() const {
    const std::shared_lock lock{mutex_};
    std::size_t total = 0;
    for (const auto& [key, samples] : series_) {
      total += samples.size();
    }
    return total;
  }

  // Returns a COPY. Handing out a reference would let a caller read the vector
  // after the lock was released, which is the whole bug this class exists to
  // prevent.
  [[nodiscard]] std::vector<Sample> samples_of(const SeriesKey& key) const {
    const std::shared_lock lock{mutex_};
    const auto it = series_.find(key);
    return it == series_.end() ? std::vector<Sample>{} : it->second;
  }

private:
  mutable std::shared_mutex mutex_;
  std::map<SeriesKey, std::vector<Sample>> series_;
};

// ---------------------------------------------------------------------------
// The ingest queue: 10.04's blocking queue, carrying batches.
// ---------------------------------------------------------------------------

class IngestQueue {
public:
  void push(std::vector<Reading> batch) {
    {
      const std::lock_guard lock{mutex_};
      batches_.push(std::move(batch));
    }
    not_empty_.notify_one();
  }

  // Waits for a batch, or returns nothing once the queue is closed AND empty.
  std::optional<std::vector<Reading>> pop() {
    std::unique_lock lock{mutex_};
    not_empty_.wait(lock, [this] { return !batches_.empty() || closed_; });

    if (batches_.empty()) {
      return std::nullopt;
    }
    auto batch = std::move(batches_.front());
    batches_.pop();
    return batch;
  }

  void close() {
    {
      const std::lock_guard lock{mutex_};
      closed_ = true;
    }
    not_empty_.notify_all();
  }

  [[nodiscard]] std::size_t depth() const {
    const std::lock_guard lock{mutex_};
    return batches_.size();
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::queue<std::vector<Reading>> batches_;
  bool closed_ = false;
};

// ---------------------------------------------------------------------------
// The pipeline: producers push batches, a pool of workers drains them into the
// store, and shutting down is cooperative (10.09).
// ---------------------------------------------------------------------------

class IngestPipeline {
public:
  IngestPipeline(ConcurrentStore& store, int worker_count) : store_(store) {
    workers_.reserve(static_cast<std::size_t>(worker_count));
    for (int i = 0; i < worker_count; ++i) {
      workers_.emplace_back([this] {
        while (auto batch = queue_.pop()) {
          store_.record_batch(*batch);
          batches_processed_.fetch_add(1, std::memory_order_relaxed);
        }
      });
    }
  }

  // Rule of five (03.05): the pipeline owns threads, so it is not copyable.
  IngestPipeline(const IngestPipeline&) = delete;
  IngestPipeline& operator=(const IngestPipeline&) = delete;
  IngestPipeline(IngestPipeline&&) = delete;
  IngestPipeline& operator=(IngestPipeline&&) = delete;

  // RAII (03.06): closing and joining is the destructor's job, so no caller
  // can forget -- and no exception can skip it.
  ~IngestPipeline() {
    drain();
  }

  void submit(std::vector<Reading> batch) {
    queue_.push(std::move(batch));
  }

  // Closes the queue and waits for every worker to finish. Idempotent, so the
  // destructor calling it after an explicit drain() is harmless.
  void drain() {
    queue_.close();
    workers_.clear(); // each jthread's destructor joins
  }

  [[nodiscard]] long batches_processed() const {
    return batches_processed_.load(std::memory_order_relaxed);
  }

private:
  ConcurrentStore& store_;
  IngestQueue queue_;
  // Relaxed is right: nothing is ordered against this counter, and it is read
  // after every worker has been joined (10.07).
  std::atomic<long> batches_processed_{0};
  std::vector<std::jthread> workers_;
};

namespace {

Timestamp at_ms(int milliseconds) {
  return Timestamp{std::chrono::milliseconds{milliseconds}};
}

std::vector<Reading> make_batch(const std::string& name, int count, int offset) {
  std::vector<Reading> batch;
  batch.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    batch.push_back({SeriesKey{name, {}}, Sample{at_ms(offset + i), double(i)}});
  }
  return batch;
}

} // namespace

TEST_CASE("the store is safe under concurrent writes") {
  ConcurrentStore store;

  {
    std::vector<std::jthread> writers;
    writers.reserve(static_cast<std::size_t>(8));
    for (int t = 0; t < 8; ++t) {
      writers.emplace_back([&store, t] {
        for (int i = 0; i < 500; ++i) {
          store.record({"metric", {{"writer", std::to_string(t)}}},
                       {at_ms(i), double(i)});
        }
      });
    }
  }

  CHECK(store.series_count() == 8);
  CHECK(store.sample_count() == 8 * 500);
}

TEST_CASE("concurrent readers and writers") {
  ConcurrentStore store;
  const SeriesKey key{"shared", {}};
  store.record(key, {at_ms(0), 0.0});

  std::atomic<long> reads{0};

  {
    std::vector<std::jthread> threads;
    threads.reserve(static_cast<std::size_t>(4));
    for (int t = 0; t < 4; ++t) {
      threads.emplace_back([&store, key] {
        for (int i = 1; i <= 250; ++i) {
          store.record(key, {at_ms(i), double(i)});
        }
      });
    }
    for (int t = 0; t < 4; ++t) {
      threads.emplace_back([&store, key, &reads] {
        for (int i = 0; i < 250; ++i) {
          // The returned vector is a copy, so it stays valid however the
          // writers change the store afterwards.
          const auto samples = store.samples_of(key);
          reads.fetch_add(static_cast<long>(samples.size()), std::memory_order_relaxed);
        }
      });
    }
  }

  CHECK(store.sample_count() == 1 + 4 * 250);
  CHECK(reads.load() > 0);
}

TEST_CASE("the pipeline processes every batch") {
  ConcurrentStore store;
  {
    IngestPipeline pipeline{store, 4};
    for (int b = 0; b < 20; ++b) {
      pipeline.submit(make_batch("cpu.load", 50, b * 50));
    }
    pipeline.drain();

    CHECK(pipeline.batches_processed() == 20);
  }

  CHECK(store.series_count() == 1);
  CHECK(store.sample_count() == 20 * 50);
}

TEST_CASE("the destructor drains, so nothing is lost when a scope is left") {
  ConcurrentStore store;
  {
    IngestPipeline pipeline{store, 2};
    for (int b = 0; b < 10; ++b) {
      pipeline.submit(make_batch("mem.used", 10, b * 10));
    }
    // No explicit drain: the destructor closes the queue and joins the
    // workers, so every submitted batch is processed before this scope ends.
  }

  CHECK(store.sample_count() == 100);
}

TEST_CASE("many producers into one pipeline") {
  ConcurrentStore store;
  {
    IngestPipeline pipeline{store, 4};

    {
      std::vector<std::jthread> producers;
      producers.reserve(static_cast<std::size_t>(4));
      for (int p = 0; p < 4; ++p) {
        producers.emplace_back([&pipeline, p] {
          for (int b = 0; b < 25; ++b) {
            pipeline.submit(make_batch("series-" + std::to_string(p), 20, b * 20));
          }
        });
      }
    }

    pipeline.drain();
    CHECK(pipeline.batches_processed() == 100);
  }

  CHECK(store.series_count() == 4);
  CHECK(store.sample_count() == 4 * 25 * 20);
}

TEST_CASE("an empty pipeline shuts down cleanly") {
  ConcurrentStore store;
  {
    IngestPipeline pipeline{store, 4};
    // Every worker is blocked in pop(); closing the queue must wake all of
    // them, not one (10.04).
  }
  CHECK(store.sample_count() == 0);
}
