// =============================================================================
//  11.01 -- Coroutines, and writing a generator
// =============================================================================
//
//  A function is a coroutine if its body contains `co_await`, `co_yield` or
//  `co_return`. The compiler then splits it into a state machine, moves its
//  locals into a heap-allocated COROUTINE FRAME, and hands you a handle to
//  resume it.
//
//  C++20 shipped the language feature and almost none of the library, so you
//  write the plumbing yourself. That plumbing is the PROMISE TYPE, found
//  through the coroutine's return type:
//
//      struct promise_type {
//        R    get_return_object();      what the caller receives
//        Awaitable initial_suspend();   run eagerly, or wait to be resumed?
//        Awaitable final_suspend();     what happens at the end
//        void return_void() / return_value(v);   how co_return works
//        void unhandled_exception();    an exception escaped the body
//        Awaitable yield_value(v);      how co_yield works (generators only)
//      };
//
//  Two decisions define the shape of a coroutine type:
//
//    initial_suspend() -> suspend_always  LAZY. Nothing runs until resumed.
//                      -> suspend_never   EAGER. The body runs up to the first
//                                         suspension before the caller sees it.
//
//    final_suspend()   -> suspend_always  the frame stays alive after the
//                                         coroutine ends, so you can read its
//                                         result -- and you must destroy() it.
//                      -> suspend_never   the frame destroys itself. Never
//                                         touch the handle afterwards.
//
//  `final_suspend` must be noexcept.
//
//  A coroutine handle is a raw resource. The type that owns one is a rule-of
//  five class (03.05): move-only, destroying the frame in its destructor.
//
//  TASK
//    Fill in `Generator`'s promise_type and its ownership, then write three
//    coroutines that use it. Everything below the promise is already written.
//
//  NOTE  This exercise starts as a compile error, and it is a long one. Work
//        top to bottom.
//
//  RUN IT
//    ./mcpp test 11_01
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

    // TODO: return a Generator wrapping this promise's handle.
    //   std::coroutine_handle<promise_type>::from_promise(*this)

    // TODO: initial_suspend -- lazy, so nothing is computed until the caller
    // asks. Return std::suspend_always.

    // TODO: final_suspend -- keep the frame alive so `done()` can be asked.
    // Must be noexcept.

    // TODO: yield_value(T) -- store the value and suspend. `co_yield v` is
    // rewritten as `co_await promise.yield_value(v)`.

    // TODO: return_void, and unhandled_exception (store it in `exception` with
    // std::current_exception(); rethrowing it from the iterator is already
    // written below).
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

  // TODO: a coroutine handle is a raw owning resource. Make Generator
  // move-only, and destroy the frame in the destructor. This is 03.05 again,
  // with std::coroutine_handle in place of the file descriptor.

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

  std::default_sentinel_t end() const noexcept {
    return {};
  }

private:
  std::coroutine_handle<promise_type> handle_{};
};

// TODO: yield every value from `first` up to but not including `last`.
Generator<int> range(int first, int last) {
  co_return;
}

// TODO: yield the Fibonacci sequence, for ever. This is the payoff: an
// infinite sequence written as an ordinary loop, and safe because nothing runs
// until it is asked for.
Generator<int> fibonacci() {
  co_return;
}

// TODO: yield each word, moving it out of `source`.
//
// Note that `source` is taken BY VALUE. A coroutine's parameters are copied
// into the frame, so a by-value parameter is safe -- a reference parameter
// would dangle the moment the caller's argument went away (11.04).
Generator<std::string> words(std::vector<std::string> source) {
  co_return;
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
  {
    auto generator = range(0, 1000000);
    auto it = generator.begin();
    ++it;
    ++it;
  }
  CHECK(true);
}
