// =============================================================================
//  10.02 -- What a data race actually is
// =============================================================================
//
//  A DATA RACE is: two threads access the same memory location, at least one
//  of them writes, and nothing orders the two accesses. It is UNDEFINED
//  BEHAVIOUR -- not "you get one of the two values", not "the count is
//  slightly off". The compiler is entitled to assume it does not happen, and
//  optimises accordingly.
//
//  `++counter` on a plain int is three operations -- load, add, store -- and
//  two threads can interleave them so an increment vanishes. That is the
//  visible symptom. The invisible one is worse: because the compiler may keep
//  `counter` in a register across a loop, a racy flag can be read once and
//  never re-read, and the loop never terminates.
//
//  The fixes, in order of preference:
//
//   1. DO NOT SHARE. Give each thread its own data and combine at the end.
//      No synchronisation is the fastest synchronisation.
//   2. std::atomic for a single value (10.06).
//   3. A mutex for anything larger, or for several values that must change
//      together (10.03).
//
//  HOW TO FIND THEM: ThreadSanitizer. It instruments memory accesses and
//  reports races with both stacks, whether or not the race happened to produce
//  a wrong answer on this run.
//
//      cmake --preset tsan && ctest --preset tsan -R 10_02
//
//  TASK
//    Fix the three races. Use a different technique for each -- the tests say
//    which.
//
//  RUN IT
//    ./mcpp test 10_02
//    ...then under tsan, which is the point of this exercise.
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

// TODO: `count` is shared and incremented without synchronisation. Make it a
// std::atomic<int>. Increments will then be indivisible, and the total exact.
int count_matching(const std::vector<int>& values, int threshold, int thread_count) {
  int count = 0;
  const std::size_t chunk = values.size() / static_cast<std::size_t>(thread_count);

  std::vector<std::jthread> workers;
  workers.reserve(static_cast<std::size_t>(thread_count));
  for (int t = 0; t < thread_count; ++t) {
    const std::size_t begin = static_cast<std::size_t>(t) * chunk;
    const std::size_t end = (t == thread_count - 1) ? values.size() : begin + chunk;
    workers.emplace_back([&values, &count, threshold, begin, end] {
      for (std::size_t i = begin; i < end; ++i) {
        if (values[i] > threshold) {
          ++count;
        }
      }
    });
  }
  workers.clear();
  return count;
}

// TODO: this appends to a shared vector from several threads -- a race on the
// vector's size, its pointer, and its buffer all at once. Give each thread its
// OWN vector and merge afterwards: no synchronisation at all in the hot loop.
std::vector<int> collect_squares(int limit, int thread_count) {
  std::vector<int> results;

  std::vector<std::jthread> workers;
  workers.reserve(static_cast<std::size_t>(thread_count));
  for (int t = 0; t < thread_count; ++t) {
    workers.emplace_back([&results, limit, thread_count, t] {
      for (int i = t; i < limit; i += thread_count) {
        results.push_back(i * i);
      }
    });
  }
  workers.clear();

  std::ranges::sort(results);
  return results;
}

// A running statistic that several threads update. Both members must change
// together, so an atomic per member would still be wrong -- a reader could see
// a new count with an old sum.
//
// TODO: protect the pair with a std::mutex.
class Statistics {
public:
  void add(int value) {
    sum_ += value;
    ++count_;
  }

  [[nodiscard]] double mean() const {
    return count_ == 0 ? 0.0 : static_cast<double>(sum_) / count_;
  }

  [[nodiscard]] int count() const {
    return count_;
  }

private:
  long sum_ = 0;
  int count_ = 0;
};

TEST_CASE("an atomic counter is exact") {
  std::vector<int> values(2000000);
  std::iota(values.begin(), values.end(), 0);

  // Everything from 1000000 upward: a million values.
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
  // Each thread adds 1 + 2 + ... + 200000.
  CHECK(statistics.mean() == doctest::Approx(100000.5));
}
