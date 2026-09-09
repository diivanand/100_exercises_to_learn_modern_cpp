// Solution -- 11.03 Awaitables
#include <doctest/doctest.h>

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// A task that can be awaited by another task.
template <typename T>
class Task {
public:
  struct promise_type {
    T value{};
    std::exception_ptr exception;
    // Who resumes when this coroutine finishes.
    std::coroutine_handle<> continuation;

    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() noexcept {
      return {};
    }

    // The awaiter returned here resumes our continuation instead of returning
    // to the resumer -- SYMMETRIC TRANSFER. Returning a handle from
    // await_suspend means "resume that one now", with no extra stack frame.
    struct FinalAwaiter {
      [[nodiscard]] bool await_ready() const noexcept {
        return false;
      }

      std::coroutine_handle<>
      await_suspend(std::coroutine_handle<promise_type> handle) noexcept {
        if (auto next = handle.promise().continuation) {
          return next;
        }
        return std::noop_coroutine();
      }

      void await_resume() const noexcept {}
    };

    FinalAwaiter final_suspend() noexcept {
      return {};
    }

    void return_value(T result) {
      value = std::move(result);
    }
    void unhandled_exception() {
      exception = std::current_exception();
    }
  };

  explicit Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
  Task& operator=(Task&& other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }
  ~Task() {
    if (handle_) {
      handle_.destroy();
    }
  }

  // Awaiting a Task: suspend the awaiting coroutine, record it as our
  // continuation, and start this one.
  auto operator co_await() {
    struct Awaiter {
      std::coroutine_handle<promise_type> handle;

      [[nodiscard]] bool await_ready() const noexcept {
        return handle.done();
      }

      std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) {
        handle.promise().continuation = awaiting;
        return handle; // symmetric transfer into the awaited task
      }

      [[nodiscard]] T await_resume() {
        if (handle.promise().exception) {
          std::rethrow_exception(handle.promise().exception);
        }
        return std::move(handle.promise().value);
      }
    };
    return Awaiter{handle_};
  }

  // Drives the task to completion from ordinary (non-coroutine) code.
  T run() {
    handle_.resume();
    if (handle_.promise().exception) {
      std::rethrow_exception(handle_.promise().exception);
    }
    return std::move(handle_.promise().value);
  }

private:
  std::coroutine_handle<promise_type> handle_{};
};

// An awaiter that never suspends: it just supplies a value. This is the
// minimal awaitable, and it shows that `co_await` is not inherently about
// asynchrony at all.
struct Immediate {
  int value;

  [[nodiscard]] bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {}
  [[nodiscard]] int await_resume() const noexcept {
    return value;
  }
};

int resume_count = 0;

// An awaiter that always suspends and resumes itself, counting the round trip.
struct CountedSuspend {
  [[nodiscard]] bool await_ready() const noexcept {
    return false;
  }

  void await_suspend(std::coroutine_handle<> handle) const {
    ++resume_count;
    handle.resume();
  }

  void await_resume() const noexcept {}
};

Task<int> leaf(int value) {
  co_return value * 2;
}

Task<int> middle(int value) {
  const int doubled = co_await leaf(value);
  co_return doubled + 1;
}

Task<int> top(int value) {
  const int from_middle = co_await middle(value);
  const int immediate = co_await Immediate{100};
  co_return from_middle + immediate;
}

Task<int> throwing() {
  throw std::runtime_error{"deep failure"};
  co_return 0;
}

// Named functions rather than lambdas, deliberately: a lambda that is a
// coroutine keeps its closure OUTSIDE the frame, so a captured reference
// dangles the moment the lambda temporary dies -- 11.04's trap, and one that
// ASan catches immediately.
Task<int> wraps_a_failure() {
  const int value = co_await throwing();
  co_return value;
}

Task<int> counts_suspensions(int times) {
  for (int i = 0; i < times; ++i) {
    co_await CountedSuspend{};
  }
  co_return times;
}

Task<std::string> collects() {
  std::string result;
  for (const auto& part : {"a", "b", "c"}) {
    result += co_await Immediate{0} == 0 ? part : "";
  }
  co_return result;
}

TEST_CASE("an awaiter that is always ready") {
  auto task = top(5);
  // leaf(5) = 10, middle = 11, plus the immediate 100.
  CHECK(task.run() == 111);
}

TEST_CASE("awaiting a task runs it and returns its value") {
  auto task = middle(7);
  CHECK(task.run() == 15);
}

TEST_CASE("await_suspend is called once per suspension") {
  resume_count = 0;
  auto task = counts_suspensions(4);
  CHECK(task.run() == 4);
  CHECK(resume_count == 4);
}

TEST_CASE("co_await in the middle of an expression") {
  auto task = collects();
  CHECK(task.run() == "abc");
}

TEST_CASE("an exception propagates out through the awaits") {
  auto outer = wraps_a_failure();
  CHECK_THROWS_AS((void)outer.run(), std::runtime_error);
}
