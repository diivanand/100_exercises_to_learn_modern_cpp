// =============================================================================
//  01.04 -- const, constexpr, and what the compiler can know
// =============================================================================
//
//  These two keywords answer different questions.
//
//    const     -- "I will not modify this through this name."
//                 An access-control promise. Says nothing about *when* the
//                 value is known.
//
//    constexpr -- "This can be computed at compile time."
//                 A promise about evaluation. On a variable it also implies
//                 const.
//
//      const int a = read_from_file();     // fine: const, but runtime
//      constexpr int b = read_from_file(); // error: not a constant expression
//
//  A `constexpr` *function* is the interesting case: it can run at compile
//  time when its arguments are constant, and at run time when they are not.
//  One definition, two lives. Since C++14 the body can contain loops, local
//  variables and branches, so this is not the crippled C++11 version.
//
//  Where it pays off: array bounds, template arguments, `static_assert`, and
//  simply moving work out of the program's runtime entirely.
//
//  TASK
//    Make `factorial` and `is_power_of_two` usable in constant expressions,
//    and give `kTableSize` a type the compiler can use as an array bound.
//
//
//  NOTE  This exercise does not compile until you have done the work. That is
//        the failing test: the whole point of `constexpr` is that the
//        *compiler* checks it, so there is nothing to run until it does.
//
//  RUN IT
//    ./mcpp test 01_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <array>
#include <cstdint>

// TODO: this is fine at run time but cannot be used in a `static_assert`.
// Mark it `constexpr`.
std::uint64_t factorial(std::uint64_t n) {
  std::uint64_t result = 1;
  for (std::uint64_t i = 2; i <= n; ++i) {
    result *= i;
  }
  return result;
}

// TODO: same treatment. A zero is not a power of two.
bool is_power_of_two(std::uint64_t n) {
  return n != 0 && (n & (n - 1)) == 0;
}

// TODO: a `const` integer initialised from a literal happens to be usable as
// a std::array size, but only because of a special rule for integral types,
// and it does not document the intent. Make it `constexpr`, which says what
// you mean and works for every type.
const std::size_t kTableSize = 8;

// Fills a compile-time-sized table with factorials. Because `factorial` is a
// constant expression, the whole table can be built before the program starts.
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
  // static_assert is checked by the compiler; if it fails, there is no test
  // run at all -- the build stops.
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
