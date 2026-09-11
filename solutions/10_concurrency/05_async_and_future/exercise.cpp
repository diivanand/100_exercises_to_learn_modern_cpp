// Solution -- 10.05 futures, promises, and std::async
#include <doctest/doctest.h>

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

long parallel_sum(const std::vector<int>& values, int parts,
                  std::span<std::thread::id> ran_on) {
  std::vector<std::future<long>> futures;
  const std::size_t chunk = values.size() / static_cast<std::size_t>(parts);

  for (int p = 0; p < parts; ++p) {
    const std::size_t begin = static_cast<std::size_t>(p) * chunk;
    const std::size_t end = (p == parts - 1) ? values.size() : begin + chunk;
    // std::launch::async, explicitly: deferred runs the task lazily on the
    // calling thread, and the default policy lets the implementation choose
    // to do the same -- neither is what "parallel" means.
    futures.push_back(std::async(std::launch::async, [&values, ran_on, p, begin, end] {
      ran_on[static_cast<std::size_t>(p)] = std::this_thread::get_id();
      long total = 0;
      for (std::size_t i = begin; i < end; ++i) {
        total += values[i];
      }
      return total;
    }));
  }

  long total = 0;
  for (auto& future : futures) {
    total += future.get();
  }
  return total;
}

int risky(bool fail) {
  if (fail) {
    throw std::runtime_error{"task failed"};
  }
  return 42;
}

std::future<int> run_risky(bool fail) {
  return std::async(std::launch::async, risky, fail);
}

void fetch_async(int id, const std::function<void(std::string)>& on_complete) {
  std::jthread worker{[id, on_complete] { on_complete("record-" + std::to_string(id)); }};
}

std::future<std::string> fetch(int id) {
  // The promise has to outlive the call, and std::function requires a
  // copyable target -- so the promise lives in a shared_ptr rather than being
  // moved into the lambda.
  auto promise = std::make_shared<std::promise<std::string>>();

  // Take the future before the promise is handed over: once the callback can
  // run, the promise may already have been satisfied and destroyed.
  std::future<std::string> future = promise->get_future();

  fetch_async(id, [promise](std::string value) { promise->set_value(std::move(value)); });

  return future;
}

TEST_CASE("parallel_sum adds up") {
  std::vector<int> values(1000);
  std::iota(values.begin(), values.end(), 1);
  std::vector<std::thread::id> ran_on(7);

  const long expected = 1000L * 1001L / 2L;
  CHECK(parallel_sum(values, 4, ran_on) == expected);
  CHECK(parallel_sum(values, 1, ran_on) == expected);
  CHECK(parallel_sum(values, 7, ran_on) == expected);
}

TEST_CASE("the chunks really run on other threads") {
  const std::vector<int> values(1000, 1);
  std::vector<std::thread::id> ran_on(4);
  CHECK(parallel_sum(values, 4, ran_on) == 1000);

  // With std::launch::async every chunk ran somewhere else. With deferred --
  // and, on some implementations, with the default policy -- they all ran
  // right here, one after another, inside get().
  for (const auto id : ran_on) {
    CHECK(id != std::this_thread::get_id());
  }
}

TEST_CASE("an exception crosses the thread boundary") {
  auto ok = run_risky(false);
  CHECK(ok.get() == 42);

  auto bad = run_risky(true);
  CHECK_THROWS_AS((void)bad.get(), std::runtime_error);
}

TEST_CASE("a promise bridges a callback to a future") {
  auto record = fetch(7);
  CHECK(record.get() == "record-7");

  CHECK(fetch(1).get() == "record-1");
}
