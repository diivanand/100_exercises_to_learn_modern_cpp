// Solution -- 10.02 What a data race actually is
#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

int count_matching(const std::vector<int>& values, int threshold, int thread_count) {
  std::atomic<int> count{0};
  const std::size_t chunk = values.size() / static_cast<std::size_t>(thread_count);

  {
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<std::size_t>(thread_count));
    for (int t = 0; t < thread_count; ++t) {
      const std::size_t begin = static_cast<std::size_t>(t) * chunk;
      const std::size_t end = (t == thread_count - 1) ? values.size() : begin + chunk;
      workers.emplace_back([&values, &count, threshold, begin, end] {
        // Counting locally and adding once at the end would be faster still --
        // one atomic operation per thread instead of one per match.
        int local = 0;
        for (std::size_t i = begin; i < end; ++i) {
          if (values[i] > threshold) {
            ++local;
          }
        }
        count.fetch_add(local, std::memory_order_relaxed);
      });
    }
  }
  return count.load();
}

std::vector<int> collect_squares(int limit, int thread_count) {
  // One output vector per thread: no shared mutable state, so no
  // synchronisation is needed inside the loop at all.
  std::vector<std::vector<int>> per_thread(static_cast<std::size_t>(thread_count));

  {
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<std::size_t>(thread_count));
    for (int t = 0; t < thread_count; ++t) {
      workers.emplace_back(
          [&slot = per_thread[static_cast<std::size_t>(t)], limit, thread_count, t] {
            for (int i = t; i < limit; i += thread_count) {
              slot.push_back(i * i);
            }
          });
    }
  }

  std::vector<int> results;
  for (const auto& slot : per_thread) {
    results.insert(results.end(), slot.begin(), slot.end());
  }
  std::ranges::sort(results);
  return results;
}

class Statistics {
public:
  void add(int value) {
    // Both members change under one lock, so no reader can see a new count
    // paired with an old sum. Two separate atomics would not give you this.
    const std::lock_guard lock{mutex_};
    sum_ += value;
    ++count_;
  }

  double mean() const {
    const std::lock_guard lock{mutex_};
    return count_ == 0 ? 0.0 : static_cast<double>(sum_) / count_;
  }

  int count() const {
    const std::lock_guard lock{mutex_};
    return count_;
  }

private:
  mutable std::mutex mutex_;
  long sum_ = 0;
  int count_ = 0;
};

TEST_CASE("an atomic counter is exact") {
  std::vector<int> values(2000000);
  std::iota(values.begin(), values.end(), 0);

  CHECK(count_matching(values, 999999, 8) == 1000000);
  CHECK(count_matching(values, 999999, 1) == 1000000);
}

TEST_CASE("not sharing at all is better than sharing carefully") {
  const auto squares = collect_squares(20000, 8);

  REQUIRE(squares.size() == 20000);
  CHECK(squares.front() == 0);
  CHECK(squares.back() == 19999 * 19999);
  CHECK(squares[2] == 4);
}

TEST_CASE("a mutex protects an invariant across two members") {
  Statistics statistics;
  {
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<std::size_t>(4));
    for (int t = 0; t < 4; ++t) {
      workers.emplace_back([&statistics] {
        for (int i = 1; i <= 200000; ++i) {
          statistics.add(i);
        }
      });
    }
  }

  CHECK(statistics.count() == 800000);
  CHECK(statistics.mean() == doctest::Approx(100000.5));
}
