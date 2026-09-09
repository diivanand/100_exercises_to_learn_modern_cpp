// =============================================================================
//  09.02 -- Captures
// =============================================================================
//
//  The capture list says what from the enclosing scope the lambda's hidden
//  class stores, and how.
//
//      [x]          a COPY of x, made when the lambda is created
//      [&x]         a REFERENCE to x. x must outlive the lambda.
//      [=]          copy everything used. Discouraged: you cannot see what was
//                   captured, and inside a member function it captures `this`.
//      [&]          reference everything used. Same objection, plus lifetimes.
//      [this]       a reference to the enclosing object (a raw pointer)
//      [*this]      a COPY of the enclosing object (C++17)
//      [x = expr]   an INIT capture: a new member initialised from anything,
//                   which is how you move into a lambda:
//                       [data = std::move(data)]
//
//  Two rules that prevent most lambda bugs (Core Guidelines F.52, F.53):
//
//   1. CAPTURE BY REFERENCE ONLY FOR LAMBDAS THAT DO NOT ESCAPE. A lambda
//      passed to `std::ranges::sort` cannot outlive the call, so `[&]` is
//      safe. One stored in a member, returned, or given to a thread can, and
//      a reference capture then dangles.
//
//   2. NAME WHAT YOU CAPTURE. `[&total]` documents the one thing that is
//      shared; `[&]` documents nothing.
//
//  `mutable` makes `operator()` non-const, so a by-value capture can be
//  modified -- the lambda then carries state between calls.
//
//  TASK
//    Fix the four captures.
//
//
//  NOTE  This exercise starts as a compile error: a lambda's operator() is
//        const by default, so the counter cannot modify what it captured.
//
//  RUN IT
//    ./mcpp test 09_02
//
//  THEN
//    Run it under `cmake --preset asan` -- `make_greeter` is a dangling
//    reference that often produces the right answer anyway.
//
// =============================================================================

#include <doctest/doctest.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

// Returns a callable that greets a name.
//
// TODO: `[&]` captures `greeting` by reference, and `greeting` is destroyed
// when the function returns. The lambda escapes, so it must own what it needs:
// capture by value, or move into an init capture.
std::function<std::string(const std::string&)> make_greeter(std::string greeting) {
  return [&](const std::string& name) { return greeting + ", " + name + "!"; };
}

// Returns a counter that yields 0, 1, 2, ... on successive calls.
//
// TODO: a by-value capture cannot be modified, because operator() is const.
// Add `mutable`. (And notice what that means: the state lives in the lambda
// object, so each counter is independent.)
std::function<int()> make_counter() {
  return [count = 0]() { return count++; };
}

// Sums the values, and records how many were odd.
//
// TODO: `[=]` copies `odd_count`, so the caller's variable never changes.
// Capture the accumulator by reference -- and only it, by name.
int sum_and_count_odd(const std::vector<int>& values, int& odd_count) {
  int total = 0;
  const auto visit = [=, &total](int value) {
    total += value;
    if (value % 2 != 0) {
      odd_count += 1;
    }
  };
  for (const int value : values) {
    visit(value);
  }
  return total;
}

class Accumulator {
public:
  explicit Accumulator(int start) : total_(start) {}

  // Returns a callable that adds to this accumulator.
  //
  // TODO: `[this]` captures a raw pointer to the enclosing object, so the
  // returned callable dangles if the Accumulator dies first. For a callable
  // that escapes, capture what it actually needs -- here, a copy of the
  // current total via `[total = total_]` -- rather than the whole object.
  //
  // (When you genuinely need the object, `[*this]` copies it, and a
  // shared_ptr + weak_ptr is the answer when it must stay shared.)
  [[nodiscard]] std::function<int(int)> adder() const {
    return [this](int value) { return total_ + value; };
  }

  [[nodiscard]] int total() const noexcept {
    return total_;
  }

private:
  int total_;
};

TEST_CASE("a lambda that escapes must own what it captured") {
  const auto greet = make_greeter("Hello");
  CHECK(greet("world") == "Hello, world!");
  CHECK(greet("again") == "Hello, again!");
}

TEST_CASE("mutable lets a lambda carry state") {
  auto counter = make_counter();
  CHECK(counter() == 0);
  CHECK(counter() == 1);
  CHECK(counter() == 2);

  // Each counter has its own state.
  auto other = make_counter();
  CHECK(other() == 0);
  CHECK(counter() == 3);
}

TEST_CASE("capture the accumulator by reference, by name") {
  int odd_count = 0;
  CHECK(sum_and_count_odd({1, 2, 3, 4, 5}, odd_count) == 15);
  CHECK(odd_count == 3);

  odd_count = 0;
  CHECK(sum_and_count_odd({2, 4}, odd_count) == 6);
  CHECK(odd_count == 0);
}

TEST_CASE("a callable that outlives its object must not capture this") {
  std::function<int(int)> add;
  {
    const Accumulator accumulator{100};
    add = accumulator.adder();
    CHECK(add(5) == 105);
  }
  // The Accumulator is gone. With `[this]`, the line below reads through a
  // dangling pointer.
  CHECK(add(5) == 105);
}
