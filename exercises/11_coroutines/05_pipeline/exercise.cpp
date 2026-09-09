// =============================================================================
//  11.05 -- Composing generators
// =============================================================================
//
//  Chapter 7 built lazy pipelines out of range adaptors. This one builds the
//  same thing out of coroutines, and the comparison is the point.
//
//      views::filter / views::transform      a range adaptor is a class
//                                            template with an iterator that
//                                            does the work in operator++
//
//      a generator coroutine                 an ordinary loop with co_yield,
//                                            and the compiler writes the
//                                            state machine
//
//  What generators buy you: RECURSION and STATE. A view that needs to remember
//  three things between elements needs three members and an iterator that
//  maintains them; a coroutine just has local variables. Try writing a
//  recursive tree traversal as a view and the difference becomes obvious.
//
//  What they cost: a heap allocation per stage (the frame), an indirect resume
//  per element, and no `size()`, no random access, no const iteration. Views
//  are usually faster; generators are usually clearer.
//
//  The design rule that makes composition safe: EACH ADAPTOR TAKES ITS SOURCE
//  BY VALUE. The pipeline then owns every stage upstream of it, and there is
//  no way for a stage to outlive the one feeding it -- the same reasoning as
//  `views::all` over an rvalue in 07.08.
//
//  TASK
//    Write the two sources, the three adaptors and the sink. `Generator` is
//    given -- it is the one you wrote in 11.01.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 11_05
//
// =============================================================================

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

  std::default_sentinel_t end() const noexcept {
    return {};
  }

private:
  std::coroutine_handle<promise_type> handle_{};
};

// Sources ---------------------------------------------------------------------

// TODO: yield first, first + 1, first + 2, ... for ever.
Generator<int> integers_from(int first) {
  co_return;
}

// TODO: yield each newline-separated line of `text`, without the newline. The
// last line has no trailing newline, and "a\n\nb" has three lines, the middle
// one empty.
//
// Note the by-value parameter -- 11.04's rule.
Generator<std::string> lines(std::string text) {
  co_return;
}

// Adaptors --------------------------------------------------------------------
//
// Each takes a Generator BY VALUE -- the adaptor owns its source, so the whole
// pipeline is one object and there is no lifetime question about the stages
// upstream of you. That is the coroutine equivalent of views::owning_view
// (07.08).

// TODO: yield only the values satisfying `predicate`.
template <typename T, typename Predicate>
Generator<T> filter(Generator<T> source, Predicate predicate) {
  co_return;
}

// TODO: yield `function(value)` for each value. The return type is already
// written for you -- working out "what does applying F to a T give?" is a
// trailing-return-type-plus-decltype job (08.01).
template <typename T, typename F>
auto transform(Generator<T> source, F function)
    -> Generator<decltype(function(std::declval<T&>()))> {
  co_return;
}

// TODO: yield at most `count` values, then stop -- which is what lets the
// whole pipeline terminate over an infinite source. Taking zero must not pull
// a single value from upstream.
template <typename T>
Generator<T> take(Generator<T> source, std::size_t count) {
  co_return;
}

// Sink ------------------------------------------------------------------------

// TODO: drain the generator into a vector. This is the only stage that is not
// lazy -- nothing happens in the pipeline until something like this pulls.
template <typename T>
std::vector<T> collect(Generator<T> source) {
  return {};
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
