// Solution -- 10.01 std::jthread
#include <doctest/doctest.h>

#include <atomic>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

void run_with_validation(std::atomic<int>& counter, bool valid) {
  // The destructor joins, on every path out of this scope.
  std::jthread worker{[&counter] { counter.fetch_add(1); }};

  if (!valid) {
    throw std::invalid_argument{"invalid input"};
  }
}

int parallel_sum(int count) {
  std::atomic<int> total{0};
  {
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      workers.emplace_back([&total, i] { total.fetch_add(i); });
    }
    // The vector's destructor destroys each jthread, and each of those joins.
    // The extra scope is what makes that happen before the load below.
  }
  return total.load();
}

void append_from_thread(std::vector<std::string>& target, std::string name) {
  std::jthread worker{
      [](std::vector<std::string>& destination, std::string value) {
        destination.push_back(std::move(value));
      },
      // std::ref is required: without it the vector is copied into the thread.
      // It is also a promise that `target` outlives the thread -- which the
      // jthread's destructor, running at the end of this function, guarantees.
      std::ref(target), std::move(name)};
}

TEST_CASE("a jthread is joined even when the scope is left by a throw") {
  std::atomic<int> counter{0};

  CHECK_NOTHROW(run_with_validation(counter, true));
  CHECK(counter.load() == 1);

  CHECK_THROWS_AS(run_with_validation(counter, false), std::invalid_argument);
  CHECK(counter.load() == 2);
}

TEST_CASE("every worker has finished before the total is read") {
  CHECK(parallel_sum(50) == 49 * 50 / 2);
  CHECK(parallel_sum(1) == 0);
  CHECK(parallel_sum(0) == 0);
}

TEST_CASE("passing a reference to a thread") {
  std::vector<std::string> names;
  append_from_thread(names, "ada");
  append_from_thread(names, "alan");

  CHECK(names == std::vector<std::string>{"ada", "alan"});
}
