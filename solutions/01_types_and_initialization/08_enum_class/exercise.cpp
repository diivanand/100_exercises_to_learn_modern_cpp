// Solution -- 01.08 Scoped enumerations
#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <type_traits>

enum class Permission : std::uint8_t {
  kNone = 0,
  kRead = 1 << 0,
  kWrite = 1 << 1,
  kExecute = 1 << 2,
};

template <typename E>
  requires std::is_enum_v<E>
constexpr auto to_underlying(E value) {
  return static_cast<std::underlying_type_t<E>>(value);
}

constexpr Permission operator|(Permission lhs, Permission rhs) {
  // The intermediate arithmetic happens in the underlying type; the result is
  // cast straight back, so no caller ever sees a bare integer.
  return static_cast<Permission>(
      static_cast<std::uint8_t>(to_underlying(lhs) | to_underlying(rhs)));
}

constexpr Permission operator&(Permission lhs, Permission rhs) {
  return static_cast<Permission>(
      static_cast<std::uint8_t>(to_underlying(lhs) & to_underlying(rhs)));
}

constexpr bool has(Permission set, Permission flag) {
  return (set & flag) == flag;
}

std::string describe(Permission set) {
  std::string result;
  if (has(set, Permission::kRead)) {
    result += 'r';
  }
  if (has(set, Permission::kWrite)) {
    result += 'w';
  }
  if (has(set, Permission::kExecute)) {
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
  CHECK(true);
}
