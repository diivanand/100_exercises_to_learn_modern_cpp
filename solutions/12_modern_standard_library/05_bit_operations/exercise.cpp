// Solution -- 12.05 <bit>, and type punning done right
#include <doctest/doctest.h>

#include <bit>
#include <cstdint>

constexpr int count_bits(std::uint32_t value) {
  // popcount already returns int.
  return std::popcount(value);
}

constexpr bool is_power_of_two(std::uint32_t value) {
  return std::has_single_bit(value);
}

constexpr std::uint32_t round_up_to_power_of_two(std::uint32_t value) {
  return std::bit_ceil(value);
}

constexpr int bits_needed(std::uint32_t value) {
  return static_cast<int>(std::bit_width(value));
}

// bit_cast checks at compile time that both types are trivially copyable and
// the same size, and unlike memcpy it works in a constant expression.
constexpr std::uint32_t float_bits(float value) {
  return std::bit_cast<std::uint32_t>(value);
}

constexpr std::uint32_t rotate_left(std::uint32_t value, int amount) {
  // std::rotl is defined for every rotation count, including 0 and 32 -- the
  // hand-written `(v << n) | (v >> (32 - n))` is undefined for both.
  return std::rotl(value, amount);
}

TEST_CASE("popcount") {
  static_assert(count_bits(0) == 0);
  static_assert(count_bits(1) == 1);
  static_assert(count_bits(0xFF) == 8);
  static_assert(count_bits(0xFFFFFFFF) == 32);
  CHECK(count_bits(0b1010) == 2);
}

TEST_CASE("powers of two") {
  static_assert(is_power_of_two(1));
  static_assert(is_power_of_two(1024));
  static_assert(!is_power_of_two(0));
  static_assert(!is_power_of_two(3));

  static_assert(round_up_to_power_of_two(0) == 1);
  static_assert(round_up_to_power_of_two(1) == 1);
  static_assert(round_up_to_power_of_two(5) == 8);
  static_assert(round_up_to_power_of_two(1024) == 1024);
  CHECK(round_up_to_power_of_two(1025) == 2048);
}

TEST_CASE("bit width") {
  static_assert(bits_needed(0) == 0);
  static_assert(bits_needed(1) == 1);
  static_assert(bits_needed(255) == 8);
  static_assert(bits_needed(256) == 9);
  CHECK(bits_needed(1000) == 10);
}

TEST_CASE("bit_cast is a constant expression, memcpy is not") {
  static_assert(float_bits(1.0F) == 0x3F800000U);
  static_assert(float_bits(0.0F) == 0x00000000U);
  static_assert(float_bits(2.0F) == 0x40000000U);

  CHECK(std::bit_cast<float>(float_bits(3.5F)) == doctest::Approx(3.5F));
}

TEST_CASE("rotate, not shift") {
  static_assert(rotate_left(0x80000000U, 1) == 0x00000001U);
  static_assert(rotate_left(0x00000001U, 1) == 0x00000002U);
  static_assert(rotate_left(0x12345678U, 8) == 0x34567812U);

  CHECK(rotate_left(0x12345678U, 0) == 0x12345678U);
  CHECK(rotate_left(0x12345678U, 32) == 0x12345678U);
}

TEST_CASE("counting zeroes") {
  static_assert(std::countr_zero(0b1000U) == 3);
  static_assert(std::countl_zero(std::uint32_t{1}) == 31);
  static_assert(std::bit_floor(100U) == 64U);
  CHECK(true);
}
