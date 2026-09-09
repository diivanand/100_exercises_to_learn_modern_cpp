// =============================================================================
//  05.06 -- std::visit and the overloaded pattern
// =============================================================================
//
//  `std::holds_alternative` chains (05.05) have a flaw: nothing checks that
//  you handled every alternative. Add a fourth type to the variant and the
//  chains keep compiling, silently taking the last branch.
//
//  `std::visit(visitor, variant)` calls the visitor with whatever is inside.
//  The visitor must be callable with EVERY alternative, so a forgotten one is
//  a compile error -- the exhaustiveness check a switch over an enum gives you,
//  for types.
//
//  A generic lambda handles them all uniformly:
//
//      std::visit([](const auto& x) { return describe(x); }, value);
//
//  But usually you want a different branch per type, which is what the
//  OVERLOADED PATTERN is for:
//
//      template <typename... Ts>
//      struct Overloaded : Ts... { using Ts::operator()...; };
//
//  Three lines that build one callable out of several lambdas, by inheriting
//  from each and pulling every `operator()` into scope. In C++20 the
//  deduction guide is implicit, so `Overloaded{lambda1, lambda2}` just works.
//  You will meet the pieces properly in chapter 08; here, use it.
//
//  `std::visit` also takes several variants at once, calling the visitor with
//  one alternative from each -- that is how you write a binary operation over
//  a variant without an N-by-N switch.
//
//  TASK
//    Implement `Overloaded`, then rewrite `type_name`, `to_string` and
//    `add` as visitors.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 05_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <variant>

using Value = std::variant<std::monostate, bool, double, std::string>;

// TODO: implement the overloaded pattern.
//
//   template <typename... Ts>
//   struct Overloaded : Ts... {
//     using Ts::operator()...;
//   };
//
// Inheriting from each lambda gives the struct all their call operators; the
// `using` declaration brings them into one overload set so that overload
// resolution can pick between them.
template <typename... Ts>
struct Overloaded;

// TODO: rewrite with std::visit and Overloaded. The point is not brevity -- it
// is that adding a fifth alternative to Value will now fail to compile here,
// instead of silently returning "string".
std::string type_name(const Value& value) {
  return "?";
}

// Renders a value the way a JSON encoder would.
//   null   -> "null"
//   bool   -> "true" / "false"
//   number -> the shortest representation that round-trips (use std::to_string
//             and trim trailing zeroes -- `trim_zeroes` below does it)
//   string -> the text in double quotes
//
// TODO: implement with std::visit.
std::string trim_zeroes(std::string text) {
  if (text.find('.') == std::string::npos) {
    return text;
  }
  while (!text.empty() && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  return text;
}

std::string to_string(const Value& value) {
  return "?";
}

// Adds two values the way a dynamically typed language might:
//   two numbers      -> their sum
//   two strings      -> concatenation
//   anything else    -> null
//
// TODO: implement with a two-variant std::visit. A generic fallback lambda
// `[](const auto&, const auto&) { return Value{}; }` catches every pair you
// did not name -- and is less specialised than the exact-type overloads, so
// they win when they apply.
Value add(const Value& lhs, const Value& rhs) {
  return Value{};
}

TEST_CASE("type_name covers every alternative") {
  CHECK(type_name(Value{}) == "null");
  CHECK(type_name(Value{true}) == "bool");
  CHECK(type_name(Value{1.0}) == "number");
  CHECK(type_name(Value{std::string{"s"}}) == "string");
}

TEST_CASE("to_string renders each alternative") {
  CHECK(to_string(Value{}) == "null");
  CHECK(to_string(Value{true}) == "true");
  CHECK(to_string(Value{false}) == "false");
  CHECK(to_string(Value{2.5}) == "2.5");
  CHECK(to_string(Value{3.0}) == "3");
  CHECK(to_string(Value{std::string{"hi"}}) == "\"hi\"");
}

TEST_CASE("add dispatches on both operands") {
  CHECK(to_string(add(Value{1.0}, Value{2.0})) == "3");
  CHECK(to_string(add(Value{std::string{"a"}}, Value{std::string{"b"}})) == "\"ab\"");

  // Mixed types have no sensible answer, so the result is null.
  CHECK(to_string(add(Value{1.0}, Value{std::string{"b"}})) == "null");
  CHECK(to_string(add(Value{true}, Value{true})) == "null");
  CHECK(to_string(add(Value{}, Value{})) == "null");
}

TEST_CASE("a visitor that misses an alternative does not compile") {
  // Uncomment this and the build fails, listing the alternative you forgot.
  // That is the guarantee std::visit buys you over a chain of ifs:
  //
  //   std::visit(Overloaded{[](double) { return 1; }}, Value{});

  CHECK(true);
}
