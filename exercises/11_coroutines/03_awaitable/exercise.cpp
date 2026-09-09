// =============================================================================
//  11.03 -- Awaitables: what co_await actually does
// =============================================================================
//
//  `co_await expr` is rewritten into calls on an AWAITER -- three functions,
//  and that is the entire protocol:
//
//      bool await_ready();
//          "Is the result already available?" If true, no suspension happens
//          at all and await_resume() is called immediately.
//
//      ??? await_suspend(std::coroutine_handle<> awaiting);
//          Called after the coroutine has suspended. `awaiting` is the handle
//          to resume when the result is ready. The return type decides what
//          happens next:
//              void / true   -- stay suspended, return to the resumer
//              false         -- resume immediately after all
//              a handle      -- SYMMETRIC TRANSFER: resume THAT coroutine
//                               instead, without growing the stack
//
//      T await_resume();
//          The value of the whole `co_await` expression. Rethrow a stored
//          exception here.
//
//  An object can be awaitable either by having those three members itself, or
//  by providing `operator co_await()` that returns something which does.
//
//  SYMMETRIC TRANSFER is the piece worth understanding. A task awaiting a task
//  awaiting a task, resumed naively, grows the stack by one frame per level --
//  and a loop of them overflows it. Returning a handle from `await_suspend`
//  tells the compiler to tail-transfer, so the stack stays flat however deep
//  the chain.
//
//  TASK
//    Implement the awaiters. `Task`'s promise and ownership are already
//    written; the awaiting is not.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 11_03
//
// =============================================================================

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
    // TODO: when this coroutine finishes, resume whoever was waiting on it.
    //
    //   * await_ready  -> false (we always want await_suspend to run)
    //   * await_suspend(handle) -> return the promise's `continuation` if
    //     there is one, or `std::noop_coroutine()` if nobody is waiting.
    //     Returning a HANDLE is symmetric transfer: it resumes that coroutine
    //     without adding a stack frame.
    //   * await_resume -> nothing
    struct FinalAwaiter {
      bool await_ready() const noexcept {
        return true;
      }
      void await_suspend(std::coroutine_handle<promise_type>) const noexcept {}
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

  // TODO: make a Task awaitable, by returning an awaiter that:
  //
  //   * await_ready   -> true only if the task has already finished
  //   * await_suspend -> record `awaiting` as this task's continuation, and
  //     return this task's handle so it starts running (symmetric transfer)
  //   * await_resume  -> rethrow a stored exception, or move out the value
  auto operator co_await() {
    struct Awaiter {
      std::coroutine_handle<promise_type> handle;

      bool await_ready() const noexcept {
        return true;
      }
      void await_suspend(std::coroutine_handle<>) const noexcept {}
      T await_resume() {
        return T{};
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

// TODO: the minimal awaitable -- one that never suspends and simply supplies
// a value. Three one-line members. Writing this one makes the protocol
// concrete, and shows that co_await is not inherently about asynchrony.
struct Immediate {
  int value;
};

int resume_count = 0;

// TODO: an awaiter that always suspends, increments `resume_count`, and then
// resumes the coroutine from inside await_suspend. That is legal and is how a
// real awaiter hands the handle to an I/O completion callback -- except that a
// real one resumes later, from another thread.
struct CountedSuspend {};

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
