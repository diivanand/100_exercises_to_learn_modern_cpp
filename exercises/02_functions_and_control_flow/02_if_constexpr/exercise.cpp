// =============================================================================
//  02.02 -- if constexpr
// =============================================================================
//
//  A normal `if` picks a branch at run time, which means *both* branches must
//  compile. Inside a template that is a real constraint: the branch that would
//  never run for a given type still has to be valid for that type.
//
//      template <typename T>
//      std::string describe(T value) {
//        if (std::is_pointer_v<T>) {
//          return describe(*value);   // must compile even when T is int
//        }                            // ...and it does not.
//        return "value";
//      }
//
//  `if constexpr` discards the branch that is not taken *before* it is
//  instantiated. The dead branch is only parsed, never type-checked against T.
//  It replaces most of what tag dispatch and SFINAE used to be for, and the
//  result reads like ordinary code.
//
//      if constexpr (std::is_pointer_v<T>) { ... } else { ... }
//
//  TASK
//    Implement `to_string` so it works for integers, floating-point values,
//    booleans and anything already string-like -- with one function and no
//    overloads.
//
//  NOTE  This exercise starts as a compile error: `std::to_string` does not
//        accept a std::string, so the single-branch version below cannot
//        instantiate for every type the tests use.
//
//  RUN IT
//    ./mcpp test 02_02
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <type_traits>

template <typename T>
std::string to_string(const T& value) {
  // TODO: dispatch on the type with `if constexpr`:
  //
  //   * bool                       -> "true" / "false"
  //   * any other integral type    -> std::to_string(value)
  //   * floating point             -> std::to_string(value)
  //   * convertible to string_view -> std::string(value)
  //
  // Order matters: `bool` is an integral type, so test for it first.
  return std::to_string(value);
}

TEST_CASE("integers") {
  CHECK(to_string(42) == "42");
  CHECK(to_string(-7) == "-7");
  CHECK(to_string(7U) == "7");
}

TEST_CASE("bool is spelled out, not printed as 1 and 0") {
  CHECK(to_string(true) == "true");
  CHECK(to_string(false) == "false");
}

TEST_CASE("floating point") {
  CHECK(to_string(1.5) == std::to_string(1.5));
}

TEST_CASE("string-like things pass through") {
  CHECK(to_string(std::string{"already"}) == "already");
  CHECK(to_string(std::string_view{"a view"}) == "a view");
  CHECK(to_string("a literal") == "a literal");
}
