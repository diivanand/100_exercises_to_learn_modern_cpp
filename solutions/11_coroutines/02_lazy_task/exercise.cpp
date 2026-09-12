// Solution -- 11.02 A lazy task
#include <doctest/doctest.h>

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

template <typename T>
class Lazy {
public:
  struct promise_type {
    T value{};
    std::exception_ptr exception;

    Lazy get_return_object() {
      return Lazy{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() noexcept {
      return {};
    }
    std::suspend_always final_suspend() noexcept {
      return {};
    }

    void return_value(T result) {
      value = std::move(result);
    }

    void unhandled_exception() {
      exception = std::current_exception();
    }
  };

  explicit Lazy(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

  Lazy(const Lazy&) = delete;
  Lazy& operator=(const Lazy&) = delete;
  Lazy(Lazy&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
  Lazy& operator=(Lazy&& other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }
  ~Lazy() {
    if (handle_) {
      handle_.destroy();
    }
  }

  // Runs the body (once) and returns its result.
  T get() {
    if (!handle_.done()) {
      handle_.resume();
    }
    if (handle_.promise().exception) {
      std::rethrow_exception(handle_.promise().exception);
    }
    return handle_.promise().value;
  }

  bool started() const {
    return handle_ && handle_.done();
  }

private:
  std::coroutine_handle<promise_type> handle_{};
};

int side_effect_count = 0;

Lazy<int> expensive(int input) {
  ++side_effect_count;
  co_return input* input;
}

Lazy<std::string> greeting(std::string name) {
  co_return "hello, " + name;
}

Lazy<int> failing() {
  throw std::runtime_error{"computation failed"};
  co_return 0;
}

// Composition without co_await: a Lazy can simply call another one's get().
Lazy<int> sum_of_squares(int a, int b) {
  auto first = expensive(a);
  auto second = expensive(b);
  co_return first.get() + second.get();
}

TEST_CASE("nothing runs until the result is asked for") {
  side_effect_count = 0;

  auto task = expensive(5);
  CHECK(side_effect_count == 0);

  CHECK(task.get() == 25);
  CHECK(side_effect_count == 1);

  // Asking again does not re-run the body.
  CHECK(task.get() == 25);
  CHECK(side_effect_count == 1);
}

TEST_CASE("a task that is never asked for never runs") {
  side_effect_count = 0;
  {
    auto ignored = expensive(9);
  }
  CHECK(side_effect_count == 0);
}

TEST_CASE("a task returning a non-trivial type") {
  auto task = greeting("ada");
  CHECK(task.get() == "hello, ada");
}

TEST_CASE("an exception in the body surfaces at get()") {
  auto task = failing();
  CHECK_THROWS_AS((void)task.get(), std::runtime_error);
}

TEST_CASE("tasks compose") {
  side_effect_count = 0;
  auto task = sum_of_squares(3, 4);
  CHECK(side_effect_count == 0);

  CHECK(task.get() == 25);
  CHECK(side_effect_count == 2);
}
