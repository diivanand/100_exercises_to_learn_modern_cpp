// =============================================================================
//  09.05 -- std::function, and what it costs
// =============================================================================
//
//  `std::function<int(int)>` is type erasure (04.09) for callables: it holds
//  anything you can call with an int that returns an int, and it is a value
//  you can copy, store in a member, and put in a vector.
//
//  What you pay for that:
//
//   * AN INDIRECT CALL. The compiler cannot inline through it.
//   * POSSIBLY AN ALLOCATION. Implementations have a small-object buffer --
//     libc++'s is a few pointers -- and a lambda capturing more than that is
//     heap-allocated on every copy.
//   * A COPY of whatever it holds, whenever it is copied.
//
//  So the rule (Core Guidelines F.50, F.51):
//
//      TEMPLATE PARAMETER when the callable is known at the call site.
//          template <typename F> void for_each(F f);
//          Inlined, zero overhead, but the function must be in a header.
//
//      std::function WHEN THE CALLABLE MUST BE STORED, or when several
//          different callables must live in the same container, or when you
//          want the function definition in a .cpp file.
//
//  C++23 adds `std::function_ref` and `std::move_only_function` for the cases
//  std::function handles badly (non-owning, and move-only callables
//  respectively). Until then, a template parameter covers the first and a
//  hand-rolled type the second.
//
//  TASK
//    Change the signatures that should be templates, keep the ones that
//    genuinely need std::function, and fix the one that cannot hold what it is
//    given.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 09_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// TODO: the callable is known at every call site, and nothing stores it.
// Make it a template parameter.
int count_matching(const std::vector<int>& values,
                   const std::function<bool(int)>& predicate) {
  int count = 0;
  for (const int value : values) {
    if (predicate(value)) {
      ++count;
    }
  }
  return count;
}

// An event system: handlers are stored, and they are all different types.
// This one genuinely needs std::function -- leave the signature alone.
class EventBus {
public:
  void subscribe(std::function<void(const std::string&)> handler) {
    handlers_.push_back(std::move(handler));
  }

  void publish(const std::string& message) const {
    for (const auto& handler : handlers_) {
      handler(message);
    }
  }

  [[nodiscard]] std::size_t handler_count() const noexcept {
    return handlers_.size();
  }

private:
  std::vector<std::function<void(const std::string&)>> handlers_;
};

// A deferred task that owns a move-only resource.
//
// TODO: std::function requires its target to be COPY constructible, so a
// lambda capturing a std::unique_ptr does not fit. Give `Task` a member that
// can hold a move-only callable.
//
// The simplest fix that keeps this exercise about the trade-off: store the
// callable in a `std::unique_ptr` to a small type-erased holder -- or, since
// this Task is only ever called once and never copied, make the member a
// template parameter and let Task be a template. Either is defensible; pick
// one and say why in a comment.
class Task {
public:
  template <typename F>
  explicit Task(F&& action) : action_(std::forward<F>(action)) {}

  void run() const {
    action_();
  }

private:
  std::function<void()> action_;
};

TEST_CASE("a template parameter costs nothing") {
  const std::vector<int> values = {1, -2, 3, -4, 5};

  CHECK(count_matching(values, [](int n) { return n > 0; }) == 3);
  CHECK(count_matching(values, [](int n) { return n < 0; }) == 2);

  // A function pointer is callable too.
  CHECK(count_matching(values, +[](int n) { return n == 3; }) == 1);
}

TEST_CASE("std::function is right when the callables must be stored together") {
  EventBus bus;
  std::vector<std::string> seen;
  int count = 0;

  bus.subscribe([&seen](const std::string& message) { seen.push_back(message); });
  bus.subscribe([&count](const std::string&) { ++count; });

  CHECK(bus.handler_count() == 2);

  bus.publish("hello");
  bus.publish("world");

  CHECK(seen == std::vector<std::string>{"hello", "world"});
  CHECK(count == 2);
}

TEST_CASE("a move-only callable does not fit in a std::function") {
  auto resource = std::make_unique<int>(42);
  int result = 0;

  // The lambda captures a unique_ptr, so it is move-only -- and std::function
  // requires copy-constructibility.
  Task task{[resource = std::move(resource), &result] { result = *resource; }};

  task.run();
  CHECK(result == 42);
}
