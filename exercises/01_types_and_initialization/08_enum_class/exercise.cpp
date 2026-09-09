// =============================================================================
//  01.08 -- Scoped enumerations
// =============================================================================
//
//  A plain `enum` has two problems. Its enumerators leak into the surrounding
//  scope, so two enums cannot both have a `kRed`. And it converts to `int`
//  implicitly, so this compiles:
//
//      enum Colour { kRed, kGreen };
//      enum Fruit  { kApple, kBanana };
//      if (kRed < kBanana) { ... }        // comparing a colour to a fruit
//      int x = kRed + 1;                  // ...and doing arithmetic on it
//
//  `enum class` fixes both: the enumerators are scoped (`Colour::kRed`) and
//  there is no implicit conversion to the underlying type. You can also fix
//  the underlying type -- `enum class Colour : std::uint8_t` -- which pins
//  down the size and makes the enum usable in a header without a definition.
//
//  Core Guidelines Enum.3: "Prefer class enums over 'plain' enums".
//
//  The cost is that when you genuinely want the number -- an array index, a
//  serialised value, a bitmask -- you must ask for it with a named cast. That
//  is the feature working, not the feature getting in your way.
//
//  TASK
//    Turn `Permission` into a scoped enum with an explicit underlying type,
//    then implement the bitmask operators it needs. Keep `to_underlying`
//    honest: it should work for any scoped enum, not just this one.
//
//
//  NOTE  This exercise starts as a compile error -- the static_asserts below
//        describe a type that does not exist yet. Work down the file and the
//        errors will run out.
//
//  RUN IT
//    ./mcpp test 01_08
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <type_traits>

// TODO: make this `enum class Permission : std::uint8_t`.
enum Permission {
  kNone = 0,
  kRead = 1 << 0,
  kWrite = 1 << 1,
  kExecute = 1 << 2,
};

// Converts any enum to its underlying integer type.
//
// C++23 has `std::to_underlying`; this is the two-line version you write
// yourself until then. `std::underlying_type_t` is the trait that answers
// "what integer is this enum really?".
//
// TODO: implement it. Constrain it to enums so a mistaken call gets a clear
// error rather than a page of template noise.
template <typename E>
constexpr auto to_underlying(E value) {
  return value;
}

// TODO: implement the bitmask operators. Scoped enums have no built-in
// arithmetic, which is exactly why you must opt in deliberately -- and why
// the opt-in is only three short functions.
constexpr Permission operator|(Permission, Permission);
constexpr Permission operator&(Permission, Permission);
constexpr bool has(Permission set, Permission flag);

std::string describe(Permission set) {
  std::string result;
  if (has(set, kRead)) {
    result += 'r';
  }
  if (has(set, kWrite)) {
    result += 'w';
  }
  if (has(set, kExecute)) {
    result += 'x';
  }
  return result.empty() ? "-" : result;
}

TEST_CASE("the underlying type is pinned down") {
  static_assert(std::is_same_v<std::underlying_type_t<Permission>, std::uint8_t>);
  static_assert(sizeof(Permission) == 1);
  CHECK(true);
}

TEST_CASE("to_underlying gives back the integer") {
  static_assert(to_underlying(Permission::kRead) == 1);
  static_assert(to_underlying(Permission::kExecute) == 4);
  static_assert(std::is_same_v<decltype(to_underlying(Permission::kRead)), std::uint8_t>);
  CHECK(true);
}

TEST_CASE("permissions compose as a bitmask") {
  constexpr Permission rw = Permission::kRead | Permission::kWrite;
  CHECK(has(rw, Permission::kRead));
  CHECK(has(rw, Permission::kWrite));
  CHECK_FALSE(has(rw, Permission::kExecute));

  CHECK(describe(rw) == "rw");
  CHECK(describe(Permission::kNone) == "-");
  CHECK(describe(Permission::kRead | Permission::kWrite | Permission::kExecute) == "rwx");
}

TEST_CASE("scoping is the point") {
  // Once Permission is a scoped enum, `kRead` on its own no longer names
  // anything -- you have to say `Permission::kRead`. Uncommenting this line
  // should stop compiling:
  //
  //   const Permission p = kRead;
  //
  // ...and so should treating it as a number:
  //
  //   const int n = Permission::kRead;
  CHECK(true);
}
