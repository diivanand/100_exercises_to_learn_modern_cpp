// =============================================================================
//  09.01 -- Lambdas
// =============================================================================
//
//  A lambda is a class with an `operator()`, written compactly. This:
//
//      auto add = [](int a, int b) { return a + b; };
//
//  is roughly this:
//
//      struct anonymous {
//        auto operator()(int a, int b) const { return a + b; }
//      };
//      anonymous add;
//
//  Two things follow from that.
//
//   * EVERY LAMBDA HAS A UNIQUE, UNNAMEABLE TYPE. Two lambdas with identical
//     bodies are different types. This is why you store them in `auto`, and
//     why passing one to a template costs nothing while passing it through
//     `std::function` costs an indirect call (09.05).
//
//   * `operator()` IS const BY DEFAULT, which is why a lambda cannot modify
//     what it captured by value unless you say `mutable` (09.02).
//
//  The anatomy:
//
//      [captures](parameters) specifiers -> ReturnType { body }
//
//  Everything except the captures and the body is optional. The return type is
//  deduced; give it explicitly when the deduction is wrong or ambiguous --
//  most often when returning a reference, since deduction strips it.
//
//  TASK
//    Write the lambdas the tests describe.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 09_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>

// TODO: a lambda that squares an int. Store it in a variable at namespace
// scope -- `inline` because this is a header-like translation unit and the
// variable would otherwise have internal linkage per TU.
inline const auto square = 0;

// TODO: a predicate: true if a string is longer than 3 characters.
inline const auto is_long = 0;

// TODO: sort `words` by length, shortest first, using a lambda comparator.
void sort_by_length(std::vector<std::string>& words) {}

// Returns a reference to the longer of two strings.
//
// TODO: write the lambda inside, and note that you must say
// `-> const std::string&` explicitly. Return type deduction uses the `auto`
// rules, which strip the reference and give you a copy.
const std::string& longer(const std::string& a, const std::string& b) {
  return a;
}

// TODO: count how many values satisfy a caller-supplied predicate. The
// predicate parameter should be a TEMPLATE parameter, not a std::function --
// that way the call is inlined and costs nothing (09.05).
int count_matching(const std::vector<int>& values) {
  return 0;
}

TEST_CASE("a lambda is a callable object") {
  CHECK(square(4) == 16);
  CHECK(square(-3) == 9);

  // Its type is unique and unnameable -- which is what `auto` is for.
  static_assert(!std::is_same_v<decltype(square), decltype(is_long)>);
  CHECK(true);
}

TEST_CASE("a lambda as a predicate") {
  CHECK(is_long(std::string{"abcd"}));
  CHECK_FALSE(is_long(std::string{"abc"}));
  CHECK_FALSE(is_long(std::string{}));
}

TEST_CASE("a lambda as a comparator") {
  std::vector<std::string> words = {"ccc", "a", "bb"};
  sort_by_length(words);
  CHECK(words == std::vector<std::string>{"a", "bb", "ccc"});
}

TEST_CASE("returning a reference needs an explicit return type") {
  const std::string a = "short";
  const std::string b = "much longer";

  CHECK(longer(a, b) == "much longer");
  // If the lambda's return type were deduced, this would compare the address
  // of a temporary copy and fail.
  CHECK(&longer(a, b) == &b);
}

TEST_CASE("taking a callable as a template parameter") {
  const std::vector<int> values = {1, -2, 3, -4, 5};

  CHECK(count_matching(values, [](int n) { return n > 0; }) == 3);
  CHECK(count_matching(values, [](int n) { return n % 2 == 0; }) == 2);
  CHECK(count_matching(values, [](int) { return false; }) == 0);
}
