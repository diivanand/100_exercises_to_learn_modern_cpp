// =============================================================================
//  10.07 -- Memory ordering
// =============================================================================
//
//  Atomicity is not the whole story. The compiler and the CPU both reorder
//  memory operations, and a value being atomic says nothing about when OTHER
//  writes become visible alongside it. Memory ordering is how you say.
//
//  The three you will actually use:
//
//    memory_order_seq_cst   the DEFAULT. Every seq_cst operation in the
//        program appears in one global order that all threads agree on. It is
//        the easiest to reason about and the most expensive. Start here.
//
//    memory_order_acquire / memory_order_release   a PAIR, and only useful as
//        a pair. A release store publishes everything the thread wrote before
//        it; an acquire load that reads that value sees all of it. This is the
//        "flag" pattern:
//
//            // producer                     // consumer
//            data = 42;                      while (!ready.load(acquire)) {}
//            ready.store(true, release);     assert(data == 42);  // holds
//
//    memory_order_relaxed   atomic, and NOTHING ELSE. No ordering with respect
//        to any other operation. Correct for a statistics counter whose value
//        nobody uses to make a decision; wrong for almost everything else.
//
//  The rule for the other 95% of code: USE THE DEFAULT. Relaxed ordering buys
//  a few nanoseconds and costs you the ability to reason about your program.
//  Reach for it only with a measurement in hand.
//
//  TASK
//    Set the three orderings at the top of the file. They are named constants
//    rather than inline arguments so that the choice is stated once -- and so
//    that a test can check it, which a race cannot reliably do.
//
//  NOTE  This exercise starts as a compile error, and it is worth
//        understanding why the tests are written this way. A missing
//        release/acquire pair is undefined behaviour, not a guaranteed wrong
//        answer: on a strongly-ordered CPU the broken version usually passes.
//        That is exactly what makes memory ordering bugs so unpleasant, and
//        why the check here is on the intent rather than on the outcome. Run
//        it under `cmake --preset tsan` as well -- ThreadSanitizer models the
//        orderings and will tell you.
//
//  RUN IT
//    ./mcpp test 10_07
//    ...and under `cmake --preset tsan`, which understands these orderings.
//
// =============================================================================

#include <doctest/doctest.h>

#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

// The orderings are named here rather than written inline, so that the choice
// is a decision you state once and the tests can check it.
//
// TODO: set these three to the right orderings.
//
//   * kPublish / kConsume must be a release/acquire PAIR -- that is what makes
//     the writes before the store visible to whoever reads the flag.
//   * kMetrics may be relaxed: nothing is ordered against those counters.
inline constexpr std::memory_order kPublish = std::memory_order_relaxed;
inline constexpr std::memory_order kConsume = std::memory_order_relaxed;
inline constexpr std::memory_order kMetrics = std::memory_order_seq_cst;

// A payload published to another thread through a flag.
struct Publication {
  std::vector<int> data;
  std::string label;
  std::atomic<bool> ready{false};

  // The writes to `data` and `label` must be visible to any thread that sees
  // `ready == true`. A relaxed store publishes the flag and promises nothing
  // about anything else, so a consumer can see `ready` true and `data` empty.
  void publish(std::vector<int> values, std::string name) {
    data = std::move(values);
    label = std::move(name);
    ready.store(true, kPublish);
  }

  [[nodiscard]] bool consume(std::vector<int>& out_data, std::string& out_label) const {
    if (!ready.load(kConsume)) {
      return false;
    }
    out_data = data;
    out_label = label;
    return true;
  }
};

// A counter nobody makes decisions from -- it is read once, after every thread
// has finished. Relaxed is exactly right here, and is the one place in this
// file where it is.
class Metrics {
public:
  void record_hit() {
    hits_.fetch_add(1, kMetrics);
  }
  void record_miss() {
    misses_.fetch_add(1, kMetrics);
  }

  long hits() const {
    return hits_.load(kMetrics);
  }
  long misses() const {
    return misses_.load(kMetrics);
  }

private:
  std::atomic<long> hits_{0};
  std::atomic<long> misses_{0};
};

// A one-shot initialisation flag. The value written before the flag must be
// visible to whoever sees the flag -- the same pair as above. Relaxed would
// let a reader see `initialised` true and `value_` still zero.
class LazyValue {
public:
  void initialise(int value) {
    value_ = value;
    initialised_.store(true, kPublish);
  }

  int get() const {
    return initialised_.load(kConsume) ? value_ : -1;
  }

private:
  int value_ = 0;
  std::atomic<bool> initialised_{false};
};

TEST_CASE("a release store publishes everything written before it") {
  for (int attempt = 0; attempt < 50; ++attempt) {
    Publication publication;
    std::vector<int> seen_data;
    std::string seen_label;
    std::atomic<bool> observed{false};

    {
      std::jthread consumer{[&] {
        std::vector<int> data;
        std::string label;
        while (!publication.consume(data, label)) {
        }
        seen_data = std::move(data);
        seen_label = std::move(label);
        observed.store(true);
      }};

      publication.publish({1, 2, 3}, "payload");
    }

    REQUIRE(observed.load());
    CHECK(seen_data == std::vector<int>{1, 2, 3});
    CHECK(seen_label == "payload");
  }
}

TEST_CASE("relaxed is right for a counter nobody synchronises on") {
  Metrics metrics;
  {
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<std::size_t>(8));
    for (int t = 0; t < 8; ++t) {
      workers.emplace_back([&metrics] {
        for (int i = 0; i < 1000; ++i) {
          if (i % 4 == 0) {
            metrics.record_miss();
          } else {
            metrics.record_hit();
          }
        }
      });
    }
  }

  // Relaxed still guarantees ATOMICITY -- the counts are exact. What it does
  // not guarantee is any ordering relative to other memory, which nothing here
  // depends on.
  CHECK(metrics.hits() == 8 * 750);
  CHECK(metrics.misses() == 8 * 250);
}

TEST_CASE("lazy initialisation seen from another thread") {
  for (int attempt = 0; attempt < 50; ++attempt) {
    LazyValue lazy;
    std::atomic<int> seen{0};

    {
      std::jthread reader{[&] {
        int value = -1;
        while (value == -1) {
          value = lazy.get();
        }
        seen.store(value);
      }};

      lazy.initialise(99);
    }

    CHECK(seen.load() == 99);
  }
}

TEST_CASE("the orderings say what was intended") {
  // A release store and an acquire load are only useful as a pair: a release
  // with a relaxed load, or an acquire with a relaxed store, orders nothing.
  static_assert(kPublish == std::memory_order_release);
  static_assert(kConsume == std::memory_order_acquire);

  // Relaxed is right here, and this is the only place in the file where it is.
  static_assert(kMetrics == std::memory_order_relaxed);
  CHECK(true);
}
