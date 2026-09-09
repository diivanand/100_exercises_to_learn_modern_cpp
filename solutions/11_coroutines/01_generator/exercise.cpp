// Solution -- 11.01 Writing a generator
#include <doctest/doctest.h>

#include <coroutine>
#include <cstddef>
#include <exception>
#include <string>
#include <utility>
#include <vector>

template <typename T>
class Generator {
public:
  struct promise_type {
    T current_value{};
    std::exception_ptr exception;

    Generator get_return_object() {
      return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    // Suspend before the body runs: nothing is computed until the first
    // increment. That is what makes the generator lazy.
    std::suspend_always initial_suspend() noexcept {
      return {};
    }

    // Suspend at the end rather than destroying the frame, so that the caller
    // can still ask `done()`. It means we own the destroy() call.
    std::suspend_always final_suspend() noexcept {
      return {};
    }

    // `co_yield v` is rewritten as `co_await promise.yield_value(v)`.
    std::suspend_always yield_value(T value) {
      current_value = std::move(value);
      return {};
    }

    void return_void() {}

    void unhandled_exception() {
      exception = std::current_exception();
    }
  };

  class Iterator {
  public:
    using value_type = T;
    using difference_type = std::ptrdiff_t;

    Iterator() = default;
    explicit Iterator(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    Iterator& operator++() {
      handle_.resume();
      if (handle_.done()) {
        rethrow_if_needed();
        handle_ = nullptr;
      }
      return *this;
    }

    void operator++(int) {
      ++*this;
    }

    const T& operator*() const {
      return handle_.promise().current_value;
    }

    bool operator==(std::default_sentinel_t) const {
      return handle_ == nullptr || handle_.done();
    }

  private:
    void rethrow_if_needed() const {
      if (handle_.promise().exception) {
        std::rethrow_exception(handle_.promise().exception);
      }
    }

    std::coroutine_handle<promise_type> handle_{};
  };

  explicit Generator(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

  // A coroutine handle is a raw resource, so Generator is a move-only owner --
  // the rule of five (03.05) applied to something the language handed us.
  Generator(const Generator&) = delete;
  Generator& operator=(const Generator&) = delete;

  Generator(Generator&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

  Generator& operator=(Generator&& other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }

  ~Generator() {
    if (handle_) {
      handle_.destroy();
    }
  }

  Iterator begin() {
    if (!handle_) {
      return Iterator{};
    }
    handle_.resume(); // run up to the first co_yield
    if (handle_.done()) {
      if (handle_.promise().exception) {
        std::rethrow_exception(handle_.promise().exception);
      }
      return Iterator{};
    }
    return Iterator{handle_};
  }

  [[nodiscard]] std::default_sentinel_t end() const noexcept {
    return {};
  }

private:
  std::coroutine_handle<promise_type> handle_{};
};

Generator<int> range(int first, int last) {
  for (int value = first; value < last; ++value) {
    co_yield value;
  }
}

Generator<int> fibonacci() {
  int a = 0;
  int b = 1;
  while (true) {
    co_yield a;
    const int next = a + b;
    a = b;
    b = next;
  }
}

Generator<std::string> words(std::vector<std::string> source) {
  for (auto& word : source) {
    co_yield std::move(word);
  }
}

template <typename T>
std::vector<T> take(Generator<T> generator, std::size_t count) {
  std::vector<T> result;
  for (const auto& value : generator) {
    if (result.size() == count) {
      break;
    }
    result.push_back(value);
  }
  return result;
}

TEST_CASE("a generator produces values on demand") {
  std::vector<int> values;
  for (const int value : range(0, 5)) {
    values.push_back(value);
  }
  CHECK(values == std::vector<int>{0, 1, 2, 3, 4});
}

TEST_CASE("an empty generator") {
  std::vector<int> values;
  for (const int value : range(0, 0)) {
    values.push_back(value);
  }
  CHECK(values.empty());
}

TEST_CASE("an infinite generator is fine, because it is lazy") {
  CHECK(take(fibonacci(), 10) == std::vector<int>{0, 1, 1, 2, 3, 5, 8, 13, 21, 34});
  CHECK(take(fibonacci(), 1) == std::vector<int>{0});
  CHECK(take(fibonacci(), 0).empty());
}

TEST_CASE("a generator of a non-trivial type") {
  const auto collected = take(words({"a", "b", "c"}), 2);
  CHECK(collected == std::vector<std::string>{"a", "b"});
}

TEST_CASE("the coroutine frame is destroyed with the generator") {
  // Abandoning a generator part-way through must not leak: the destructor
  // destroys the frame, running the destructors of everything alive in it.
  {
    auto generator = range(0, 1000000);
    auto it = generator.begin();
    ++it;
    ++it;
  }
  CHECK(true);
}
