// Solution -- 10.04 Condition variables
#include <doctest/doctest.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

template <typename T>
class BlockingQueue {
public:
  void push(T value) {
    {
      // The state change happens under the lock. Notifying outside it is a
      // small optimisation (the woken thread does not immediately block on a
      // mutex we still hold), but setting it outside would be a lost wakeup.
      const std::lock_guard lock{mutex_};
      items_.push(std::move(value));
    }
    not_empty_.notify_one();
  }

  std::optional<T> pop() {
    std::unique_lock lock{mutex_};

    // The predicate form loops for us, so a spurious wake changes nothing.
    not_empty_.wait(lock, [this] { return !items_.empty() || closed_; });

    // Closed but not yet empty still yields items: a consumer drains before
    // it stops.
    if (items_.empty()) {
      return std::nullopt;
    }

    T value = std::move(items_.front());
    items_.pop();
    return value;
  }

  void close() {
    {
      const std::lock_guard lock{mutex_};
      closed_ = true;
    }
    // Every waiter's predicate has just become true, so every waiter must be
    // woken. notify_one would strand the rest.
    not_empty_.notify_all();
  }

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

  CHECK(total.load() == 4 * 250 * 251 / 2);
}
