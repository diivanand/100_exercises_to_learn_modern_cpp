// =============================================================================
//  10.04 -- Condition variables
// =============================================================================
//
//  A condition variable lets a thread WAIT for something another thread will
//  make true, without burning CPU in a spin loop.
//
//  The shape is always the same, and every part of it matters:
//
//      // waiter
//      std::unique_lock lock{mutex};
//      cv.wait(lock, [&] { return ready; });   // predicate, not a bare wait
//      // ...the lock is held again here, and `ready` is true
//
//      // notifier
//      {
//        std::lock_guard lock{mutex};
//        ready = true;
//      }
//      cv.notify_one();
//
//  Why each piece:
//
//   * THE PREDICATE OVERLOAD. `wait` can return spuriously -- with nothing
//     changed and nobody having notified. The predicate form loops until the
//     condition holds, so a spurious wake is invisible. Never use the bare
//     `wait(lock)`.
//
//   * unique_lock, NOT lock_guard. `wait` unlocks the mutex while it sleeps
//     and re-locks it before returning, so it needs a lock it can operate.
//
//   * SET THE STATE UNDER THE LOCK. Otherwise the waiter can check the
//     predicate, find it false, and go to sleep in the gap before the notify.
//     That is the lost-wakeup bug, and it hangs.
//
//   * notify_one vs notify_all: one when any single waiter can make progress,
//     all when the state change may satisfy several different predicates.
//
//  TASK
//    Implement a blocking queue. It is the canonical use, and getting it right
//    once is worth more than reading about it ten times.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 10_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

template <typename T>
class BlockingQueue {
public:
  // TODO: push a value and wake exactly one waiter.
  void push(T value) {}

  // TODO: wait until there is an item or the queue is closed. Return the item,
  // or std::nullopt once the queue is closed AND empty -- that combination is
  // what lets a consumer loop terminate.
  std::optional<T> pop() {
    return std::nullopt;
  }

  // TODO: mark the queue closed and wake EVERY waiter, so they can all see
  // that no more items are coming. This is the notify_all case.
  void close() {}

  [[nodiscard]] std::size_t size() const {
    const std::lock_guard lock{mutex_};
    return items_.size();
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::queue<T> items_;
  bool closed_ = false;
};

TEST_CASE("a consumer waits for a producer") {
  BlockingQueue<int> queue;
  std::vector<int> received;

  {
    std::jthread consumer{[&queue, &received] {
      while (const auto value = queue.pop()) {
        received.push_back(*value);
      }
    }};

    for (int i = 0; i < 100; ++i) {
      queue.push(i);
    }
    queue.close();
  }

  REQUIRE(received.size() == 100);
  CHECK(received.front() == 0);
  CHECK(received.back() == 99);
}

TEST_CASE("closing an empty queue releases the waiters") {
  BlockingQueue<int> queue;
  std::atomic<int> finished{0};

  {
    std::vector<std::jthread> consumers;
    consumers.reserve(static_cast<std::size_t>(4));
    for (int t = 0; t < 4; ++t) {
      consumers.emplace_back([&queue, &finished] {
        while (queue.pop()) {
        }
        finished.fetch_add(1);
      });
    }

    // Every consumer is now blocked in pop(). notify_all is what wakes them
    // all; notify_one would leave three of them asleep forever.
    queue.close();
  }

  CHECK(finished.load() == 4);
}

TEST_CASE("items already queued are drained before the queue reports closed") {
  BlockingQueue<int> queue;
  queue.push(1);
  queue.push(2);
  queue.close();

  CHECK(queue.pop() == 1);
  CHECK(queue.pop() == 2);
  CHECK_FALSE(queue.pop().has_value());
  CHECK_FALSE(queue.pop().has_value());
}

TEST_CASE("many producers and many consumers") {
  BlockingQueue<int> queue;
  std::atomic<long> total{0};

  {
    std::vector<std::jthread> consumers;
    consumers.reserve(static_cast<std::size_t>(4));
    for (int t = 0; t < 4; ++t) {
      consumers.emplace_back([&queue, &total] {
        while (const auto value = queue.pop()) {
          total.fetch_add(*value);
        }
      });
    }

    {
      std::vector<std::jthread> producers;
      producers.reserve(static_cast<std::size_t>(4));
      for (int t = 0; t < 4; ++t) {
        producers.emplace_back([&queue] {
          for (int i = 1; i <= 250; ++i) {
            queue.push(i);
          }
        });
      }
    }
    queue.close();
  }

  // Four producers, each pushing 1 + 2 + ... + 250.
  CHECK(total.load() == 4 * 250 * 251 / 2);
}
