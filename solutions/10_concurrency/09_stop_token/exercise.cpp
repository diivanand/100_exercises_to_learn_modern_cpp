// Solution -- 10.09 Cooperative cancellation with std::stop_token
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>

int count_until_stopped(std::chrono::milliseconds run_for) {
  std::atomic<int> iterations{0};

  {
    // A jthread passes its stop_token to a callable that asks for one.
    std::jthread worker{[&iterations](std::stop_token token) {
      while (!token.stop_requested()) {
        iterations.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
      }
    }};

    std::this_thread::sleep_for(run_for);
    // The destructor calls request_stop() and then joins -- so this scope
    // exits cleanly with nothing written at the call site.
  }

  return iterations.load();
}

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
    thread_ = std::jthread{[this](std::stop_token token) {
      while (!token.stop_requested()) {
        std::unique_lock lock{mutex_};

        // condition_variable_any's stop_token overload returns false when the
        // wait ended because of a stop request rather than the predicate. A
        // plain condition_variable cannot do this, which is why the member is
        // an `_any`.
        if (!not_empty_.wait(lock, token, [this] { return !items_.empty(); })) {
          return;
        }

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

  [[nodiscard]] long processed() const {
    return processed_.load();
  }

private:
  std::mutex mutex_;
  std::condition_variable_any not_empty_;
  std::queue<int> items_;
  std::atomic<long> processed_{0};
  std::jthread thread_;
};

bool runs_a_callback_on_stop() {
  std::atomic<bool> cleaned_up{false};
  std::stop_source source;

  const std::stop_token token = source.get_token();
  const std::stop_callback callback{token, [&cleaned_up] { cleaned_up.store(true); }};

  source.request_stop();

  return cleaned_up.load();
}

TEST_CASE("a jthread stops when its destructor asks it to") {
  const int iterations = count_until_stopped(std::chrono::milliseconds{30});
  CHECK(iterations > 0);
}

TEST_CASE("a blocked worker can still be cancelled") {
  Worker worker;
  worker.start();

  for (int i = 1; i <= 10; ++i) {
    worker.submit(i);
  }

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

  CHECK(ran);
}
