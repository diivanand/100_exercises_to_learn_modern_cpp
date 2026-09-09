// =============================================================================
//  13.05 -- Capstone: concurrent ingestion
// =============================================================================
//
//  The last exercise. Producers hand batches of readings to a pipeline, a pool
//  of workers drains them into the store, and none of it may lose a sample,
//  race, deadlock, or hang on the way out.
//
//  Everything chapter 10 covered, in one place:
//
//   * A shared_mutex, because queries greatly outnumber writes (10.03) -- and
//     `mutable`, so the const accessors can lock it (04.01).
//
//   * BATCHING UNDER ONE LOCK. Locking once per sample is the difference
//     between a store that scales and one that does not.
//
//   * RETURNING A COPY from `samples_of`. A reference would be readable after
//     the lock was released, which is precisely the bug this class exists to
//     prevent -- and is the reason the answer is not simply "return a view".
//
//   * A blocking queue (10.04), with close() as the shutdown signal, so
//     workers drain what is queued and then stop.
//
//   * jthreads owned by the pipeline (10.01), joined by its destructor (03.06)
//     -- so leaving the scope cannot lose a batch, whatever happened.
//
//   * Relaxed ordering for the counter, and only for the counter (10.07).
//
//  TASK
//    Implement ConcurrentStore, IngestQueue and IngestPipeline.
//
//  NOTE  This exercise starts as a compile error. Run it under
//        `cmake --preset tsan` when it passes -- a concurrency test that
//        passes is weak evidence; ThreadSanitizer is strong evidence.
//
//  RUN IT
//    ./mcpp test 13_05
//
// =============================================================================

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
  // TODO: record one sample. An exclusive lock -- this is a write.
  void record(const SeriesKey& key, Sample sample) {}

  // TODO: record a whole batch under ONE lock. Locking per sample would be
  // correct and slow; this is the version worth writing.
  void record_batch(const std::vector<Reading>& readings) {}

  // TODO: series_count and sample_count, both const, both under a SHARED lock
  // so concurrent readers do not serialise behind each other.
  [[nodiscard]] std::size_t series_count() const {
    return 0;
  }
  [[nodiscard]] std::size_t sample_count() const {
    return 0;
  }

  // TODO: the samples of one series.
  //
  // Return a COPY, not a reference. A reference would outlive the lock, and a
  // reader holding it while a writer appends is a data race -- the exact bug
  // this whole class exists to prevent. This is the one place in the course
  // where copying is the correct answer rather than the lazy one.
  [[nodiscard]] std::vector<Sample> samples_of(const SeriesKey& key) const {
    return {};
  }

private:
  // TODO: a mutable std::shared_mutex, and the map.
  mutable std::shared_mutex mutex_;
  std::map<SeriesKey, std::vector<Sample>> series_;
};

// ---------------------------------------------------------------------------
// The ingest queue: 10.04's blocking queue, carrying batches.
// ---------------------------------------------------------------------------

class IngestQueue {
public:
  // TODO: push a batch and wake one waiter. Set the state under the lock and
  // notify outside it (10.04).
  void push(std::vector<Reading> batch) {}

  // TODO: wait for a batch. Return nothing once the queue is closed AND empty
  // -- that combination is what lets a worker loop terminate.
  //
  // Use the PREDICATE overload of wait: the bare one can return spuriously.
  std::optional<std::vector<Reading>> pop() {
    return std::nullopt;
  }

  // TODO: mark the queue closed and wake EVERY waiter. notify_one here would
  // strand the other workers for ever.
  void close() {}

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
  // TODO: start `worker_count` jthreads, each looping on queue_.pop() until it
  // returns nothing, recording each batch and counting it.
  IngestPipeline(ConcurrentStore& store, int worker_count) : store_(store) {}

  // TODO: a pipeline owns running threads, so copying and moving it make no
  // sense. Say so (03.05).

  // TODO: the destructor must drain -- close the queue and join every worker.
  // That is what makes the "no explicit drain" test pass, and it is RAII doing
  // for threads what it does for memory (03.06).
  ~IngestPipeline() {}

  void submit(std::vector<Reading> batch) {
    queue_.push(std::move(batch));
  }

  // TODO: close the queue and wait for the workers. It must be safe to call
  // twice, since the destructor calls it too.
  void drain() {}

  [[nodiscard]] long batches_processed() const {
    return batches_processed_.load(std::memory_order_relaxed);
  }

private:
  ConcurrentStore& store_;
  IngestQueue queue_;
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
    batch.push_back(
        {SeriesKey{name, {}}, Sample{at_ms(offset + i), static_cast<double>(i)}});
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
                       {at_ms(i), static_cast<double>(i)});
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
      // NOLINTNEXTLINE(bugprone-exception-escape)
      threads.emplace_back([&store, key] {
        for (int i = 1; i <= 250; ++i) {
          store.record(key, {at_ms(i), static_cast<double>(i)});
        }
      });
    }
    for (int t = 0; t < 4; ++t) {
      // NOLINTNEXTLINE(bugprone-exception-escape)
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
