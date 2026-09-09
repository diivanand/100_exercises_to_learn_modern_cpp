// =============================================================================
//  09.04 -- constexpr lambdas
// =============================================================================
//
//  Since C++17 a lambda's `operator()` is implicitly `constexpr` whenever it
//  could be. So this already works:
//
//      constexpr auto square = [](int n) { return n * n; };
//      static_assert(square(4) == 16);
//
//  You can say `constexpr` explicitly to get an error if the body is not
//  actually usable in a constant expression -- the same reason you write
//  `override` (04.04): so the compiler checks your intention rather than
//  silently doing something else.
//
//  And `consteval` (01.05) works too, forcing every call to compile time.
//
//  Where this matters: a lambda passed to a `constexpr` algorithm, a
//  compile-time table built by a lambda, a comparator used in a `constexpr`
//  sort. In C++20 most of the standard algorithms are constexpr, so an
//  entire pipeline can run before the program does.
//
//  The catch: a captured variable has to be usable in a constant expression
//  too, and a lambda capturing a reference to a runtime value cannot be.
//
//  TASK
//    Make the lambdas below usable at compile time, and build a table with one.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 09_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

// TODO: mark it constexpr, so the compiler checks it really is.
inline const auto cube = [](int n) { return n * n * n; };

// TODO: a constexpr lambda that counts the vowels in a string_view. Loops and
// local variables are allowed in a constant expression since C++14.
inline const auto count_vowels = [](std::string_view) { return 0; };

// TODO: build a compile-time table by calling a lambda. The whole function is
// consteval, so the table exists before main does.
template <std::size_t N>
consteval std::array<int, N> cube_table() {
  return {};
}

// TODO: a constexpr comparator, used with std::ranges::sort at compile time.
// (std::sort and std::ranges::sort are constexpr in C++20.)
consteval std::array<int, 5> sorted_descending(std::array<int, 5> values) {
  return values;
}

TEST_CASE("a lambda in a constant expression") {
  static_assert(cube(3) == 27);
  static_assert(cube(-2) == -8);
  CHECK(cube(4) == 64);
}

TEST_CASE("a constexpr lambda with a loop") {
  static_assert(count_vowels("hello") == 2);
  static_assert(count_vowels("") == 0);
  static_assert(count_vowels("xyz") == 0);
  static_assert(count_vowels("aeiou") == 5);
  CHECK(count_vowels("modern") == 2);
}

TEST_CASE("a table built by a lambda, before the program runs") {
  constexpr auto table = cube_table<5>();
  static_assert(table[0] == 0);
  static_assert(table[2] == 8);
  static_assert(table[4] == 64);
  CHECK(table[3] == 27);
}

TEST_CASE("sorting at compile time") {
  constexpr auto sorted = sorted_descending({3, 1, 4, 1, 5});
  static_assert(sorted[0] == 5);
  static_assert(sorted[4] == 1);
  CHECK(sorted[1] == 4);
}

TEST_CASE("the same lambda still works at run time") {
  int runtime_value = 5;
  CHECK(cube(runtime_value) == 125);

  // A constexpr function that is called with runtime arguments is simply an
  // ordinary function. That is the difference from consteval (01.05).
  runtime_value = 2;
  CHECK(cube(runtime_value) == 8);
}
