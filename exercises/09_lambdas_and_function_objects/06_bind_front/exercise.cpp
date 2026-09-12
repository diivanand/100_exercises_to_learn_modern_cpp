// =============================================================================
//  09.06 -- std::bind_front, std::invoke, and the death of std::bind
// =============================================================================
//
//  `std::bind` (C++11) was a way to fix some of a function's arguments. It was
//  also confusing, slow to compile, and full of surprises -- placeholders that
//  reorder arguments, nested binds that compose in non-obvious ways, silent
//  copies. Do not use it (Core Guidelines: prefer lambdas).
//
//  A lambda does the same job, readably:
//
//      auto add5 = [](int n) { return add(5, n); };            // instead of
//      auto add5 = std::bind(add, 5, std::placeholders::_1);   // this
//
//  C++20's `std::bind_front(f, args...)` covers the common case -- fix the
//  FIRST arguments, forward the rest -- without the placeholder machinery, and
//  is often shorter than the lambda when the trailing arguments are many or
//  their types are awkward to name.
//
//  `std::invoke(f, args...)` is the other piece. It calls anything callable
//  the same way, including the two things that are not calls at all:
//
//      std::invoke(f, a, b)                    f(a, b)
//      std::invoke(&Class::method, obj, a)     obj.method(a)
//      std::invoke(&Class::field, obj)         obj.field
//
//  That uniformity is why projections (07.02) accept `&Person::name` as
//  happily as a lambda: everything in the standard library that "calls
//  something" is specified in terms of std::invoke.
//
//  TASK
//    Replace the std::bind calls, and write the two small utilities.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 09_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

struct Logger {
  std::string prefix;
  std::vector<std::string> lines;

  void log(const std::string& level, const std::string& message) {
    lines.push_back(prefix + " [" + level + "] " + message);
  }

  std::size_t count() const {
    return lines.size();
  }
};

int scaled_sum(int factor, int a, int b) {
  return factor * (a + b);
}

// TODO: return a callable that adds `amount` to its argument, using
// std::bind_front over an ordinary function.
int add(int a, int b) {
  return a + b;
}

auto adder(int amount) {
  return 0;
}

// TODO: return a callable that calls `logger.log("info", message)`.
// std::bind_front works with a pointer-to-member-function: the first bound
// argument becomes the object.
auto info_logger(Logger& logger) {
  return 0;
}

// TODO: call `f` with `arguments`, whatever kind of callable it is -- a
// function, a lambda, a pointer to member function, or a pointer to member
// data. std::invoke is the one spelling that covers all four.
template <typename F, typename... Args>
decltype(auto) call(F&& f, Args&&... arguments) {
  return 0;
}

TEST_CASE("bind_front fixes the leading arguments") {
  const auto add5 = adder(5);
  CHECK(add5(1) == 6);
  CHECK(add5(-5) == 0);

  // It works for any number of remaining arguments.
  const auto triple_sum = std::bind_front(scaled_sum, 3);
  CHECK(triple_sum(1, 2) == 9);
}

TEST_CASE("bind_front over a member function") {
  Logger logger{"app", {}};
  const auto info = info_logger(logger);

  info("started");
  info("ready");

  CHECK(logger.count() == 2);
  CHECK(logger.lines[0] == "app [info] started");
  CHECK(logger.lines[1] == "app [info] ready");
}

TEST_CASE("invoke calls anything the same way") {
  Logger logger{"svc", {}};

  // a free function
  CHECK(call(add, 2, 3) == 5);

  // a lambda
  CHECK(call([](int n) { return n * 10; }, 4) == 40);

  // a pointer to member function
  call(&Logger::log, logger, "warn", "disk full");
  CHECK(logger.lines.at(0) == "svc [warn] disk full");

  // a pointer to member DATA -- not a call at all, but invoke handles it
  CHECK(call(&Logger::prefix, logger) == "svc");
}

TEST_CASE("a lambda is still often the clearest option") {
  // bind_front is not always shorter. Reordering arguments, for instance, it
  // cannot do at all -- and that is fine, because this reads better anyway:
  const auto reversed_subtract = [](int a, int b) { return b - a; };
  CHECK(reversed_subtract(3, 10) == 7);
}
