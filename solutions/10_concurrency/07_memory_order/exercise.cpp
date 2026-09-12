// Solution -- 10.07 Memory ordering
#include <doctest/doctest.h>

#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Naming the orderings once, where the decision is made, beats scattering
// memory_order arguments through the code.
inline constexpr std::memory_order kPublish = std::memory_order_release;
inline constexpr std::memory_order kConsume = std::memory_order_acquire;
inline constexpr std::memory_order kMetrics = std::memory_order_relaxed;

struct Publication {
  std::vector<int> data;
  std::string label;
  std::atomic<bool> ready{false};

  void publish(std::vector<int> values, std::string name) {
    data = std::move(values);
    label = std::move(name);
    // Release: everything this thread wrote above becomes visible to any
    // thread that acquires this flag and reads `true`.
    ready.store(true, kPublish);
  }

  [[nodiscard]] bool consume(std::vector<int>& out_data, std::string& out_label) const {
    // Acquire: pairs with the release above. Without both halves, neither
    // does anything.
    if (!ready.load(kConsume)) {
      return false;
    }
    out_data = data;
    out_label = label;
    return true;
  }
};

class Metrics {
public:
  // Relaxed: atomic, unordered, and nothing depends on when these become
  // visible relative to anything else. The counts are still exact.
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
