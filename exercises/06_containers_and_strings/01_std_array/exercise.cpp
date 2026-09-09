// =============================================================================
//  06.01 -- std::array
// =============================================================================
//
//  A C array is a second-class type: it decays to a pointer at the first
//  opportunity, does not know its own size once it has, cannot be assigned,
//  cannot be returned from a function, and has no bounds checking at all.
//
//  `std::array<T, N>` is the same storage -- a fixed-size block, no heap, no
//  indirection -- with a real type wrapped round it:
//
//      .size()      known at compile time, and constexpr
//      .at(i)       bounds-checked, throws std::out_of_range
//      .front()/.back()/.begin()/.end()
//      assignment, comparison, and returning by value all work
//      it is an aggregate, so `std::array<int, 3> a{1, 2, 3}` works
//
//  It is also fully usable at compile time, which makes it the natural home
//  for a `constexpr` lookup table (01.04).
//
//  Two details worth knowing:
//
//   * `std::array<T, 0>` is legal, and `begin() == end()`.
//   * Class template argument deduction works: `std::array a{1, 2, 3}` deduces
//     `std::array<int, 3>`.
//
//  TASK
//    Replace the C arrays below with std::array, and build a compile-time
//    lookup table.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 06_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <numeric>
#include <stdexcept>

// TODO: return a std::array<int, 3> instead of taking an out-parameter. A C
// array cannot be returned by value, which is why this signature exists at
// all -- and why it is so easy to pass the wrong size.
void first_three_squares(int out[3]) {
  for (int i = 0; i < 3; ++i) {
    out[i] = (i + 1) * (i + 1);
  }
}

// TODO: take a `const std::array<int, N>&` (a template on N) so the size
// travels with the data. The version below has already lost it: `values` is a
// pointer, and `count` is a promise the caller may break.
int sum(const int* values, std::size_t count) {
  int total = 0;
  for (std::size_t i = 0; i < count; ++i) {
    total += values[i];
  }
  return total;
}

// A compile-time table of the first N triangular numbers: 0, 1, 3, 6, 10, ...
//
// TODO: implement it. The whole table should be built by the compiler, which
// is why the return type is a std::array and the function is consteval.
template <std::size_t N>
consteval std::array<int, N> triangular_numbers() {
  return {};
}

TEST_CASE("an array knows its own size") {
  const std::array squares = first_three_squares();
  static_assert(squares.size() == 3);
  CHECK(squares[0] == 1);
  CHECK(squares[1] == 4);
  CHECK(squares[2] == 9);
}

TEST_CASE("the size travels with the data") {
  const std::array<int, 4> values = {1, 2, 3, 4};
  CHECK(sum(values) == 10);

  const std::array<int, 2> pair = {10, 20};
  CHECK(sum(pair) == 30);

  const std::array<int, 0> nothing = {};
  CHECK(sum(nothing) == 0);
}

TEST_CASE("at() is bounds-checked, operator[] is not") {
  std::array<int, 3> values = {1, 2, 3};
  CHECK(values.at(2) == 3);
  CHECK_THROWS_AS((void)values.at(3), std::out_of_range);

  // `values[3]` would be undefined behaviour -- no exception, no crash you can
  // count on. Build this exercise with `cmake --preset asan` and try it.
}

TEST_CASE("arrays are values") {
  std::array<int, 3> a = {1, 2, 3};
  const std::array<int, 3> b = a;
  a[0] = 99;

  CHECK(b[0] == 1);
  CHECK(a != b);
  CHECK(b == std::array<int, 3>{1, 2, 3});
}

TEST_CASE("the triangular table is built at compile time") {
  constexpr auto table = triangular_numbers<6>();
  static_assert(table.size() == 6);
  static_assert(table[0] == 0);
  static_assert(table[1] == 1);
  static_assert(table[2] == 3);
  static_assert(table[5] == 15);
  CHECK(table[4] == 10);
}
