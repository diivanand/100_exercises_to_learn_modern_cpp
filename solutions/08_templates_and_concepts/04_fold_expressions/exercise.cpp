// Solution -- 08.04 Fold expressions
#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

template <typename... Ts>
constexpr auto sum(const Ts&... values) {
  // Binary left fold: ((0 + a) + b) + c. The 0 makes the empty pack valid.
  return (0 + ... + values);
}

template <typename... Ts>
constexpr bool all_true(const Ts&... values) {
  return (... && values);
}

template <typename... Ts>
constexpr bool any_true(const Ts&... values) {
  return (... || values);
}

template <typename T, typename... Ts>
void push_all(std::vector<T>& target, const Ts&... values) {
  // A comma fold: the operands are evaluated left to right, and the result is
  // discarded. The idiomatic way to "do this for each argument".
  (target.push_back(static_cast<T>(values)), ...);
}

template <typename T, typename... Ts>
constexpr bool is_one_of(const T& needle, const Ts&... candidates) {
  // The folded pattern is `needle == candidates`, not just `candidates`.
  return (... || (needle == candidates));
}

template <typename... Ts>
std::string concat(const Ts&... values) {
  // The std::string{} seed is what makes `"a" + "b"` legal: the left operand of
  // every + is a std::string.
  return (std::string{} + ... + values);
}

TEST_CASE("summing a pack") {
  static_assert(sum() == 0);
  static_assert(sum(1, 2, 3) == 6);
  CHECK(sum(1.5, 2.5) == doctest::Approx(4.0));
  CHECK(sum(1, 2.5) == doctest::Approx(3.5));
}

TEST_CASE("logical folds, including over an empty pack") {
  static_assert(all_true());
  static_assert(!any_true());

  static_assert(all_true(true, true));
  static_assert(!all_true(true, false));
  static_assert(any_true(false, true));
  static_assert(!any_true(false, false));
  CHECK(true);
}

TEST_CASE("a comma fold performs an action per argument, in order") {
  std::vector<int> values;
  push_all(values, 1, 2, 3);
  CHECK(values == std::vector<int>{1, 2, 3});

  push_all(values);
  CHECK(values.size() == 3);
}

TEST_CASE("folding over an expression, not just the pack") {
  static_assert(is_one_of(2, 1, 2, 3));
  static_assert(!is_one_of(9, 1, 2, 3));
  static_assert(!is_one_of(1));

  CHECK(is_one_of(std::string{"b"}, "a", "b", "c"));
}

TEST_CASE("concat, without the recursion") {
  CHECK(concat() == "");
  CHECK(concat("a", "b", "c") == "abc");
  CHECK(concat(std::string{"x"}, "y") == "xy");
}
