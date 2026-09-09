// Solution -- 09.04 constexpr lambdas
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

inline constexpr auto cube = [](int n) constexpr { return n * n * n; };

inline constexpr auto count_vowels = [](std::string_view text) constexpr {
  int count = 0;
  for (const char c : text) {
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
      ++count;
    }
  }
  return count;
};

template <std::size_t N>
consteval std::array<int, N> cube_table() {
  std::array<int, N> table{};
  for (std::size_t i = 0; i < N; ++i) {
    table[i] = cube(static_cast<int>(i));
  }
  return table;
}

consteval std::array<int, 5> sorted_descending(std::array<int, 5> values) {
  // std::ranges::sort is constexpr in C++20, and so is the lambda.
  std::ranges::sort(values, [](int a, int b) constexpr { return a > b; });
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

  runtime_value = 2;
  CHECK(cube(runtime_value) == 8);
}
