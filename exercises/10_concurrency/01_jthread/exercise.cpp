// =============================================================================
//  10.01 -- std::jthread
// =============================================================================
//
//  `std::thread` has a design flaw: its destructor calls std::terminate if the
//  thread was neither joined nor detached. So every scope holding one needs a
//  join on every exit path, including the ones an exception takes -- which is
//  RAII's job, and std::thread does not do it.
//
//  `std::jthread` (C++20) is std::thread with the destructor fixed: it
//  requests a stop and joins. The "j" is for joining.
//
//      {
//        std::jthread worker{[] { work(); }};
//      }   // joined here, whatever happened
//
//  Use it by default (Core Guidelines CP.25). Reach for std::thread only when
//  you genuinely want to detach -- which is rarely, because a detached thread
//  outliving the data it refers to is a use-after-free waiting to happen.
//
//  Arguments are COPIED into the thread by default. To pass a reference you
//  have to say so with `std::ref`, and then you own the lifetime problem:
//
//      std::jthread t{work, std::ref(shared)};   // `shared` must outlive t
//
//  A jthread also carries a `std::stop_token` for cooperative cancellation --
//  that is 10.09.
//
//  TASK
//    Fix the three thread bugs.
//
//  RUN IT
//    ./mcpp test 10_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// TODO: use std::jthread so the thread is joined even when `validate` throws.
// As written, the throw skips the join and the std::thread destructor calls
// std::terminate -- the process dies rather than the exception propagating.
void run_with_validation(std::atomic<int>& counter, bool valid) {
  std::thread worker{[&counter] { counter.fetch_add(1); }};

  if (!valid) {
    throw std::invalid_argument{"invalid input"};
  }

  worker.join();
}

// Runs `count` workers, each adding its index to the total.
//
// TODO: the threads are started but never joined, so `total` is read while
// they are still writing to it -- and the vector of threads is destroyed with
// threads still running. Use jthreads, and make sure they have all finished
// before the total is read.
int parallel_sum(int count) {
  std::atomic<int> total{0};
  std::vector<std::thread> workers;
  for (int i = 0; i < count; ++i) {
    workers.emplace_back([&total, i] { total.fetch_add(i); });
  }
  return total.load();
}

// Appends a name from another thread.
//
// TODO: thread arguments are copied, so this appends to a COPY of `target` and
// the caller's vector stays empty. Pass a reference explicitly with std::ref
// -- and note that doing so makes the lifetime your problem, which is why the
// join has to happen before `target` goes out of scope.
void append_from_thread(std::vector<std::string>& target, std::string name) {
  std::jthread worker{[](std::vector<std::string> destination, std::string value) {
                        destination.push_back(std::move(value));
                      },
                      target, std::move(name)};
}

TEST_CASE("a jthread is joined even when the scope is left by a throw") {
  std::atomic<int> counter{0};

  CHECK_NOTHROW(run_with_validation(counter, true));
  CHECK(counter.load() == 1);

  CHECK_THROWS_AS(run_with_validation(counter, false), std::invalid_argument);
  // The thread ran and was joined on the way out, so the count is exact.
  CHECK(counter.load() == 2);
}

TEST_CASE("every worker has finished before the total is read") {
  // 0 + 1 + ... + 49
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
