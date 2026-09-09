// Solution -- 06.01 std::array
#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <stdexcept>

std::array<int, 3> first_three_squares() {
  std::array<int, 3> result{};
  for (int i = 0; i < 3; ++i) {
    result[static_cast<std::size_t>(i)] = (i + 1) * (i + 1);
  }
  return result;
}

// N is deduced from the argument, so there is no count to get wrong.
template <std::size_t N>
int sum(const std::array<int, N>& values) {
  int total = 0;
  for (const int value : values) {
    total += value;
  }
  return total;
}

template <std::size_t N>
consteval std::array<int, N> triangular_numbers() {
  std::array<int, N> table{};
  int running = 0;
  for (std::size_t i = 0; i < N; ++i) {
    table[i] = running;
    running += static_cast<int>(i) + 1;
  }
  return table;
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
