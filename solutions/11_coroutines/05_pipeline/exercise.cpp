// Solution -- 11.05 Composing generators
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
    std::suspend_always initial_suspend() noexcept {
      return {};
    }
    std::suspend_always final_suspend() noexcept {
      return {};
    }
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
        if (handle_.promise().exception) {
          std::rethrow_exception(handle_.promise().exception);
        }
        handle_ = nullptr;
      }
      return *this;
    }
    void operator++(int) {
      ++*this;
    }

    T& operator*() const {
      return handle_.promise().current_value;
    }

    bool operator==(std::default_sentinel_t) const {
      return handle_ == nullptr || handle_.done();
    }

  private:
    std::coroutine_handle<promise_type> handle_{};
  };

  explicit Generator(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
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
    handle_.resume();
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

// Sources ---------------------------------------------------------------------

Generator<int> integers_from(int first) {
  for (int value = first;; ++value) {
    co_yield value;
  }
}

Generator<std::string> lines(std::string text) {
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t newline = text.find('\n', start);
    if (newline == std::string::npos) {
      co_yield text.substr(start);
      co_return;
    }
    co_yield text.substr(start, newline - start);
    start = newline + 1;
  }
}

// Adaptors --------------------------------------------------------------------
//
// Each takes a Generator BY VALUE -- the adaptor owns its source, so the whole
// pipeline is one object and there is no lifetime question about the stages
// upstream of you. That is the coroutine equivalent of views::owning_view
// (07.08).

template <typename T, typename Predicate>
Generator<T> filter(Generator<T> source, Predicate predicate) {
  for (auto& value : source) {
    if (predicate(value)) {
      co_yield std::move(value);
    }
  }
}

template <typename T, typename F>
auto transform(Generator<T> source, F function)
    -> Generator<decltype(function(std::declval<T&>()))> {
  for (auto& value : source) {
    co_yield function(value);
  }
}

template <typename T>
Generator<T> take(Generator<T> source, std::size_t count) {
  if (count == 0) {
    co_return;
  }
  std::size_t taken = 0;
  for (auto& value : source) {
    co_yield std::move(value);
    if (++taken == count) {
      co_return;
    }
  }
}

// Sink ------------------------------------------------------------------------

template <typename T>
std::vector<T> collect(Generator<T> source) {
  std::vector<T> result;
  for (auto& value : source) {
    result.push_back(std::move(value));
  }
  return result;
}

TEST_CASE("a three-stage pipeline over an infinite source") {
  const auto result =
      collect(take(transform(filter(integers_from(1), [](int n) { return n % 3 == 0; }),
                             [](int n) { return n * n; }),
                   4));

  CHECK(result == std::vector<int>{9, 36, 81, 144});
}

TEST_CASE("each stage is lazy, so only what is needed is computed") {
  int predicate_calls = 0;
  int transform_calls = 0;

  const auto result = collect(take(transform(filter(integers_from(1),
                                                    [&predicate_calls](int) {
                                                      ++predicate_calls;
                                                      return true;
                                                    }),
                                             [&transform_calls](int n) {
                                               ++transform_calls;
                                               return n;
                                             }),
                                   3));

  CHECK(result == std::vector<int>{1, 2, 3});
  CHECK(transform_calls == 3);
  CHECK(predicate_calls == 3);
}

TEST_CASE("a pipeline over text") {
  const auto result =
      collect(transform(filter(lines("alpha\n\nbeta\ngamma"),
                               [](const std::string& line) { return !line.empty(); }),
                        [](const std::string& line) { return line.size(); }));

  CHECK(result == std::vector<std::size_t>{5, 4, 5});
}

TEST_CASE("taking nothing runs nothing") {
  int calls = 0;
  const auto result = collect(take(transform(integers_from(1),
                                             [&calls](int n) {
                                               ++calls;
                                               return n;
                                             }),
                                   0));
  CHECK(result.empty());
  CHECK(calls == 0);
}

TEST_CASE("a finite source ends the pipeline") {
  const auto result = collect(take(lines("one\ntwo"), 10));
  CHECK(result == std::vector<std::string>{"one", "two"});
}
