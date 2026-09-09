// =============================================================================
//  10.05 -- futures, promises, and std::async
// =============================================================================
//
//  A `std::future<T>` is a handle to a value that is not ready yet. Calling
//  `get()` blocks until it is, and then hands it over -- or rethrows whatever
//  exception the producer threw, which is the feature threads do not have.
//
//  Three ways to get one:
//
//    std::async(policy, f, args...)
//        Run f, get a future for its result. TWO POLICIES:
//          std::launch::async     -- on a new thread, guaranteed
//          std::launch::deferred  -- lazily, on the thread that calls get()
//        THE DEFAULT IS `async | deferred`, which means the implementation
//        chooses -- and may choose deferred, so your "parallel" code runs
//        sequentially. ALWAYS PASS std::launch::async EXPLICITLY.
//
//    std::promise<T>
//        The producing end, for when the value does not come from a function
//        return -- a callback, an event, a completion handler.
//
//    std::packaged_task<Sig>
//        A callable that fills in a future when invoked. The building block of
//        a thread pool.
//
//  THE OTHER TRAP: the future returned by `std::async` has a destructor that
//  BLOCKS until the task finishes. So `std::async(...);` as a statement is
//  synchronous -- the temporary future is destroyed at the semicolon.
//
//  TASK
//    Fix the three problems, and use a promise to bridge a callback API.
//
//  RUN IT
//    ./mcpp test 10_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// TODO: force these onto real threads with std::launch::async. With the
// default policy an implementation may defer every one of them, and then they
// run one after another inside the get() loop.
long parallel_sum(const std::vector<int>& values, int parts) {
  std::vector<std::future<long>> futures;
  const std::size_t chunk = values.size() / static_cast<std::size_t>(parts);

  for (int p = 0; p < parts; ++p) {
    const std::size_t begin = static_cast<std::size_t>(p) * chunk;
    const std::size_t end = (p == parts - 1) ? values.size() : begin + chunk;
    futures.push_back(std::async([&values, begin, end] {
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

// An exception thrown in the task is stored in the future and rethrown by
// get(). That is worth knowing: a std::thread has no such mechanism, and an
// exception escaping a thread function calls std::terminate.
int risky(bool fail) {
  if (fail) {
    throw std::runtime_error{"task failed"};
  }
  return 42;
}

// TODO: run `risky` asynchronously and return the future.
std::future<int> run_risky([[maybe_unused]] bool fail) {
  return {};
}

// A callback-based API we do not control.
void fetch_async(int id, const std::function<void(std::string)>& on_complete) {
  std::jthread worker{[id, on_complete] { on_complete("record-" + std::to_string(id)); }};
}

// TODO: bridge it to a future with a std::promise. Create the promise, take
// its future BEFORE handing the promise to the callback, and set the value
// from inside the callback.
//
// (The promise must outlive the callback, which is why it is captured by
// value into a shared_ptr or moved into the lambda -- think about which.)
std::future<std::string> fetch([[maybe_unused]] int id) {
  return {};
}

TEST_CASE("parallel_sum adds up") {
  std::vector<int> values(1000);
  std::iota(values.begin(), values.end(), 1);

  const long expected = 1000L * 1001L / 2L;
  CHECK(parallel_sum(values, 4) == expected);
  CHECK(parallel_sum(values, 1) == expected);
  CHECK(parallel_sum(values, 7) == expected);
}

TEST_CASE("the tasks really do run on other threads") {
  // Each task reports the thread it ran on. With std::launch::async they must
  // all differ from this one.
  std::vector<std::future<std::thread::id>> futures;
  futures.reserve(4);
  for (int i = 0; i < 4; ++i) {
    futures.push_back(
        std::async(std::launch::async, [] { return std::this_thread::get_id(); }));
  }

  for (auto& future : futures) {
    CHECK(future.get() != std::this_thread::get_id());
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
