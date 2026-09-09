// =============================================================================
//  12.05 -- <bit>, and type punning done right
// =============================================================================
//
//  Reinterpreting one type's bits as another's is something every low-level
//  programmer needs and almost everybody does wrong:
//
//      float f = 1.0F;
//      int i = *reinterpret_cast<int*>(&f);          // UB: strict aliasing
//      union { float f; int i; } u; u.f = 1.0F; ... // UB in C++ (fine in C)
//      std::memcpy(&i, &f, sizeof f);               // correct, but not
//                                                   // constexpr, and verbose
//
//  `std::bit_cast<To>(from)` (C++20) is the right answer: it copies the bits,
//  it is a constant expression, and it requires both types to be trivially
//  copyable and the same size -- checked at compile time.
//
//  `<bit>` also has the operations CPUs have had for decades and C++ had no
//  spelling for:
//
//      std::popcount(x)        how many bits are set
//      std::countl_zero(x)     leading zeroes
//      std::countr_zero(x)     trailing zeroes
//      std::bit_width(x)       bits needed to represent x
//      std::bit_ceil(x)        the next power of two at or above x
//      std::bit_floor(x)       the previous power of two at or below x
//      std::has_single_bit(x)  is it a power of two?
//      std::rotl / std::rotr   rotate
//      std::endian::native     what byte order this machine uses
//
//  All of them are constexpr, and all compile to a single instruction where
//  the hardware has one. They require an UNSIGNED integer type -- which is the
//  library refusing to guess what you meant by a shift on a negative number.
//
//  TASK
//    Replace the hand-written bit twiddling.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 12_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <bit>
#include <cstdint>
#include <cstring>

// TODO: std::popcount.
constexpr int count_bits(std::uint32_t value) {
  int count = 0;
  while (value != 0) {
    count += value & 1U;
    value >>= 1U;
  }
  return count;
}

// TODO: std::has_single_bit. Note that it says false for 0, which is what you
// want -- zero is not a power of two.
constexpr bool is_power_of_two(std::uint32_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

// TODO: std::bit_ceil. Rounding a size up to the next power of two is what
// every hash table and every allocator does, and getting the loop right
// (including the overflow case) is fiddly enough to be worth not doing.
constexpr std::uint32_t round_up_to_power_of_two(std::uint32_t value) {
  if (value <= 1) {
    return 1;
  }
  std::uint32_t result = 1;
  while (result < value) {
    result <<= 1U;
  }
  return result;
}

// TODO: std::bit_width -- the number of bits needed to represent `value`,
// which is also floor(log2(value)) + 1.
constexpr int bits_needed(std::uint32_t value) {
  int bits = 0;
  while (value != 0) {
    ++bits;
    value >>= 1U;
  }
  return bits;
}

// TODO: std::bit_cast. The memcpy version is correct but is not a constant
// expression, so the static_asserts below cannot use it.
constexpr std::uint32_t float_bits(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof value);
  return bits;
}

// TODO: std::rotl -- a rotate, not a shift: the bits that fall off the top
// come back in at the bottom.
constexpr std::uint32_t rotate_left(std::uint32_t value, int amount) {
  return (value << amount) | (value >> (32 - amount));
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
  // IEEE-754: 1.0f is 0x3F800000.
  static_assert(float_bits(1.0F) == 0x3F800000U);
  static_assert(float_bits(0.0F) == 0x00000000U);
  static_assert(float_bits(2.0F) == 0x40000000U);

  // ...and it round-trips.
  CHECK(std::bit_cast<float>(float_bits(3.5F)) == doctest::Approx(3.5F));
}

TEST_CASE("rotate, not shift") {
  static_assert(rotate_left(0x80000000U, 1) == 0x00000001U);
  static_assert(rotate_left(0x00000001U, 1) == 0x00000002U);
  static_assert(rotate_left(0x12345678U, 8) == 0x34567812U);

  // A rotation by 0, or by the full width, is the identity -- and the
  // hand-written version above has undefined behaviour for both, because
  // shifting by 32 is UB. std::rotl handles them.
  CHECK(rotate_left(0x12345678U, 0) == 0x12345678U);
  CHECK(rotate_left(0x12345678U, 32) == 0x12345678U);
}

TEST_CASE("counting zeroes") {
  static_assert(std::countr_zero(0b1000U) == 3);
  static_assert(std::countl_zero(std::uint32_t{1}) == 31);
  static_assert(std::bit_floor(100U) == 64U);
  CHECK(true);
}
