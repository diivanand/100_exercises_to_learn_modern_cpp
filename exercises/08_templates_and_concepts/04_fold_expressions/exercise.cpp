// =============================================================================
//  08.04 -- Fold expressions (C++17)
// =============================================================================
//
//  A fold applies a binary operator across a parameter pack, with no recursion
//  and no base case. Four forms:
//
//      (pack op ...)         unary  right fold:  a op (b op c)
//      (... op pack)         unary  left  fold:  (a op b) op c
//      (init op ... op pack) binary right fold
//      (pack op ... op init) binary left  fold
//
//  Read the `...` as "and so on in this direction". The `init` forms exist
//  because a unary fold over an EMPTY pack is only valid for three operators:
//  `&&` gives true, `||` gives false, `,` gives void. For everything else --
//  `+` included -- you must supply the initial value.
//
//  Prefer LEFT folds: `(... + pack)` evaluates left to right, which is the
//  order a reader expects, and matters for non-associative operations.
//
//  The comma fold is the general-purpose one:
//
//      (f(values), ...);            call f with each, in order
//      ((os << values << ' '), ...);
//
//  TASK
//    Rewrite these with folds. Compare each against the recursive version you
//    wrote in 08.03 -- the fold is not just shorter, it instantiates one
//    function instead of N.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 08_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

// TODO: sum them. Use a binary fold so that summing nothing is 0.
template <typename... Ts>
constexpr auto sum(const Ts&... values) {
  return 0;
}

// TODO: true if every argument is true. A unary && fold, which is already
// correct for an empty pack.
template <typename... Ts>
constexpr bool all_true(const Ts&... values) {
  return false;
}

// TODO: true if any argument is true.
template <typename... Ts>
constexpr bool any_true(const Ts&... values) {
  return false;
}

// TODO: push every argument onto the vector, in order. A comma fold.
template <typename T, typename... Ts>
void push_all(std::vector<T>& target, const Ts&... values) {}

// TODO: true if `needle` equals any of the other arguments. A fold over `||`
// with a comparison inside -- note that the pattern being folded can be any
// expression mentioning the pack, not just the pack itself.
template <typename T, typename... Ts>
constexpr bool is_one_of(const T& needle, const Ts&... candidates) {
  return false;
}

// TODO: concatenate. A binary left fold starting from an empty std::string, so
// that the first `+` has a std::string on the left and the whole thing does not
// try to add two const char*s.
template <typename... Ts>
std::string concat(const Ts&... values) {
  return {};
}

TEST_CASE("summing a pack") {
  static_assert(sum() == 0);
  static_assert(sum(1, 2, 3) == 6);
  CHECK(sum(1.5, 2.5) == doctest::Approx(4.0));
  CHECK(sum(1, 2.5) == doctest::Approx(3.5));
}

TEST_CASE("logical folds, including over an empty pack") {
  static_assert(all_true());  // vacuously true
  static_assert(!any_true()); // vacuously false

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
