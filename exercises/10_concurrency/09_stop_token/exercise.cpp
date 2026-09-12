// =============================================================================
//  10.09 -- Cooperative cancellation with std::stop_token
// =============================================================================
//
//  There is no safe way to kill a thread from outside. Whatever it was in the
//  middle of stays in the middle -- a held lock, a half-written buffer, a
//  destructor that never runs. So cancellation in C++ is COOPERATIVE: you ask,
//  and the thread checks.
//
//  C++20 standardises the asking:
//
//      std::stop_source    the asker.   request_stop()
//      std::stop_token     the askee.   stop_requested(), and it is copyable
//      std::stop_callback  a function that runs when a stop is requested --
//                          which is how you interrupt a thread that is
//                          blocked rather than looping
//
//  A `std::jthread` (10.01) has one built in: give its callable a
//  `std::stop_token` first parameter and the thread supplies it, and the
//  jthread destructor calls `request_stop()` before joining. So a well-written
//  jthread worker shuts down cleanly with no code at the call site at all.
//
//      std::jthread worker{[](std::stop_token token) {
//        while (!token.stop_requested()) { work(); }
//      }};
//      // destructor: request_stop(), then join
//
//  For a thread blocked on a condition variable, use
//  `std::condition_variable_any` and its `wait(lock, token, predicate)`
//  overload, which returns when the predicate holds OR a stop is requested.
//
//  TASK
//    Make the three workers cancellable.
//
//  NOTE  This exercise compiles as it stands, and then never finishes: the
//        workers have no way to be told to stop. `./mcpp test` kills it
//        after 30 seconds and says so. That hang is the bug.
//
//  RUN IT
//    ./mcpp test 10_09
//
// =============================================================================

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

// TODO: give the lambda a `std::stop_token` first parameter and loop until a
// stop is requested. As written this never terminates, and the jthread
// destructor would block forever waiting to join it.
//
// Return how many iterations it managed.
int count_until_stopped(std::chrono::milliseconds run_for) {
  std::atomic<int> iterations{0};

  {
    std::jthread worker{[&iterations] {
      while (true) {
        iterations.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
      }
    }};

    std::this_thread::sleep_for(run_for);
  }

  return iterations.load();
}

// A worker blocked waiting for items. A plain stop_requested() check never
// runs while it is asleep in wait().
//
// TODO: use std::condition_variable_any and the three-argument wait that takes
// a stop_token, so a stop request wakes the thread.
class Worker {
public:
  void submit(int value) {
    {
      const std::lock_guard lock{mutex_};
      items_.push(value);
    }
    not_empty_.notify_one();
  }

  void start() {
    thread_ = std::jthread{[this] {
      while (true) {
        std::unique_lock lock{mutex_};
        not_empty_.wait(lock, [this] { return !items_.empty(); });

        const int value = items_.front();
        items_.pop();
        lock.unlock();

        processed_.fetch_add(value);
      }
    }};
  }

  void stop() {
    thread_.request_stop();
  }

  long processed() const {
    return processed_.load();
  }

private:
  std::mutex mutex_;
  std::condition_variable_any not_empty_;
  std::queue<int> items_;
  std::atomic<long> processed_{0};
  std::jthread thread_;
};

// TODO: register a std::stop_callback so that `cleaned_up` is set when the
// stop is requested -- without the worker having to poll for it.
bool runs_a_callback_on_stop() {
  std::atomic<bool> cleaned_up{false};
  std::stop_source source;

  const std::stop_token token = source.get_token();
  source.request_stop();

  return cleaned_up.load();
}

TEST_CASE("a jthread stops when its destructor asks it to") {
  const int iterations = count_until_stopped(std::chrono::milliseconds{30});

  // It ran, and it stopped -- if it had not, this test would never return.
  CHECK(iterations > 0);
}

TEST_CASE("a blocked worker can still be cancelled") {
  Worker worker;
  worker.start();

  for (int i = 1; i <= 10; ++i) {
    worker.submit(i);
  }

  // Wait for the work to be picked up, then cancel while the worker is asleep
  // in wait() with nothing left to do.
  while (worker.processed() < 55) {
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }

  worker.stop();
  CHECK(worker.processed() == 55);
}

TEST_CASE("a stop_callback runs without polling") {
  CHECK(runs_a_callback_on_stop());
}

TEST_CASE("a callback registered after the stop runs immediately") {
  std::stop_source source;
  source.request_stop();

  bool ran = false;
  const std::stop_callback callback{source.get_token(), [&ran] { ran = true; }};

  // Registering on an already-stopped token invokes the callback on the spot,
  // which is what makes this safe to use without racing the request.
  CHECK(ran);
}
