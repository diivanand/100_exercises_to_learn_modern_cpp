// =============================================================================
//  11.02 -- A lazy task
// =============================================================================
//
//  A generator (11.01) yields many values. A TASK produces one, and the
//  difference in the promise type is small: `return_value` instead of
//  `yield_value`, and no iterator.
//
//  What a lazy task buys you:
//
//   * The body does not run until somebody wants the answer. Build a
//     computation, pass it around, discard it unused -- nothing happened.
//   * The result is computed once and cached in the frame.
//   * An exception in the body is stored and rethrown where the result is
//     asked for -- the same trick std::future uses (10.05).
//
//  This is what an async task type looks like underneath, minus the part that
//  makes it asynchronous. A real one suspends on I/O and resumes when the I/O
//  completes; the promise machinery is the same, and `await_suspend` is where
//  the difference lives (11.03).
//
//  Note what `co_return` does NOT do: it does not return from the coroutine to
//  its caller with a value. It calls `promise.return_value(...)` and then
//  reaches `final_suspend`. The caller gets whatever `get_return_object`
//  produced, long before.
//
//  TASK
//    Fill in Lazy's promise type and ownership, then write the coroutines.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 11_02
//
// =============================================================================

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

    // TODO: get_return_object, initial_suspend (lazy), final_suspend
    // (noexcept, suspend_always so the result survives), unhandled_exception.
    //
    // TODO: `return_value(T)` rather than the generator's `yield_value` --
    // that one difference is most of what separates a task from a generator.
  };

  explicit Lazy(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

  // TODO: move-only ownership of the handle, as in 11.01.

  // Runs the body (once) and returns its result.
  //
  // TODO: resume the coroutine if it has not finished, rethrow a stored
  // exception if there is one, and return the promise's value. Calling get()
  // twice must not run the body twice -- `handle_.done()` is how you know.
  T get() {
    return T{};
  }

  bool started() const {
    return handle_ && handle_.done();
  }

private:
  std::coroutine_handle<promise_type> handle_{};
};

int side_effect_count = 0;

// TODO: increment side_effect_count, then co_return the square. The counter is
// how the tests prove the body did not run early.
Lazy<int> expensive(int input) {
  co_return 0;
}

// TODO: co_return "hello, " followed by the name.
Lazy<std::string> greeting(std::string name) {
  co_return "";
}

// TODO: throw a std::runtime_error. It must surface from get(), not from the
// call that created the task -- the body has not run yet at that point.
//
// (The unreachable `co_return` is what makes this a coroutine at all: the
// compiler decides from the body, not the return type.)
Lazy<int> failing() {
  co_return 0;
}

// TODO: build two `expensive` tasks and co_return the sum of their results.
// Neither inner task should run until this one does.
Lazy<int> sum_of_squares(int a, int b) {
  co_return 0;
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
