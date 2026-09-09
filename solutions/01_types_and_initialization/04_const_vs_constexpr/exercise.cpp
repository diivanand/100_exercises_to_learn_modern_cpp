// Solution -- 01.04 const vs constexpr
#include <doctest/doctest.h>

#include <array>
#include <cstdint>

constexpr std::uint64_t factorial(std::uint64_t n) {
  std::uint64_t result = 1;
  for (std::uint64_t i = 2; i <= n; ++i) {
    result *= i;
  }
  return result;
}

constexpr bool is_power_of_two(std::uint64_t n) {
  return n != 0 && (n & (n - 1)) == 0;
}

constexpr std::size_t kTableSize = 8;

constexpr std::array<std::uint64_t, kTableSize> make_factorial_table() {
  std::array<std::uint64_t, kTableSize> table{};
  for (std::size_t i = 0; i < kTableSize; ++i) {
    table[i] = factorial(i);
  }
  return table;
}

TEST_CASE("factorial works at run time") {
  CHECK(factorial(0) == 1);
  CHECK(factorial(5) == 120);
  CHECK(factorial(20) == 2432902008176640000ULL);
}

TEST_CASE("factorial works at compile time") {
  static_assert(factorial(0) == 1);
  static_assert(factorial(10) == 3628800);
  CHECK(true);
}

TEST_CASE("is_power_of_two") {
  static_assert(is_power_of_two(1));
  static_assert(is_power_of_two(1024));
  static_assert(!is_power_of_two(0));
  static_assert(!is_power_of_two(12));
  CHECK(is_power_of_two(64));
}

TEST_CASE("the table is built before main runs") {
  constexpr auto table = make_factorial_table();
  static_assert(table.size() == 8);
  static_assert(table[7] == 5040);
  CHECK(table[3] == 6);
}
