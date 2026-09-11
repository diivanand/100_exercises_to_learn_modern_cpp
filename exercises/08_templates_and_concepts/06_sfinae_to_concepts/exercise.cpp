// =============================================================================
//  08.06 -- From SFINAE to concepts
// =============================================================================
//
//  Before C++20, constraining a template meant exploiting a rule with an
//  unfortunate name: SUBSTITUTION FAILURE IS NOT AN ERROR. If substituting the
//  deduced types into a signature produces something invalid, that overload is
//  quietly dropped from the overload set rather than making the program
//  ill-formed.
//
//  So you wrote deliberately-invalid signatures:
//
//      template <typename T,
//                typename = std::enable_if_t<std::is_integral_v<T>>>
//      void f(T value);
//
//  It works. It is also unreadable, produces error messages measured in
//  screenfuls, and cannot express "T must have a `size()` member" without more
//  machinery (the detection idiom).
//
//  C++20 concepts do the same job as a readable predicate:
//
//      template <std::integral T>
//      void f(T value);
//
//  The standard library ships a set in <concepts>: `std::integral`,
//  `std::floating_point`, `std::same_as`, `std::convertible_to`,
//  `std::derived_from`, `std::invocable`, `std::equality_comparable`,
//  `std::totally_ordered`, and the range concepts you met in chapter 07.
//
//  A concept also ORDERS overloads: a more constrained overload wins over a
//  less constrained one, which replaces the tag-dispatch trick entirely.
//
//  TASK
//    Rewrite the SFINAE below with concepts. The behaviour must not change --
//    only the readability, and the error messages.
//
//  NOTE  This exercise starts as a compile error: the last test names the two
//        concepts you are about to write.
//
//  RUN IT
//    ./mcpp test 08_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// TODO: replace with `template <std::integral T>`.
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
std::string describe(T) {
  return "integer";
}

// TODO: replace with `template <std::floating_point T>`.
//
// Note that with SFINAE these two overloads need different signatures to avoid
// redeclaring the same template; with concepts they do not.
template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>,
          typename = void>
std::string describe(T) {
  return "floating point";
}

// TODO: `std::convertible_to<std::string_view>`.
template <typename T,
          typename = std::enable_if_t<std::is_convertible_v<T, std::string_view>>,
          typename = void, typename = void>
std::string describe(T) {
  return "string-like";
}

// TODO: express "T has a size() member returning something convertible to
// std::size_t" as a concept, and constrain this on it. With SFINAE that
// needed the detection idiom; as a concept it is three lines.
//
//   template <typename T>
//   concept Sized = requires(const T& value) {
//     { value.size() } -> std::convertible_to<std::size_t>;
//   };
template <typename T>
std::size_t size_of(const T& value) {
  return value.size();
}

// TODO: constrain this so it accepts only types that are BOTH sized and
// support `operator[]`. Concepts compose with `&&` -- and the composition is
// itself a concept you can name.
template <typename T>
auto first_element(const T& container) {
  return container[0];
}

TEST_CASE("overload resolution picks by concept") {
  CHECK(describe(42) == "integer");
  CHECK(describe(42L) == "integer");
  CHECK(describe('c') == "integer");
  CHECK(describe(1.5) == "floating point");
  CHECK(describe(1.5F) == "floating point");
  CHECK(describe("literal") == "string-like");
  CHECK(describe(std::string{"owned"}) == "string-like");
}

TEST_CASE("a concept about a member, not a type trait") {
  CHECK(size_of(std::string{"abc"}) == 3);
  CHECK(size_of(std::vector<int>{1, 2}) == 2);

  // An int has no size(), so this does not compile -- and the error names the
  // requirement rather than dumping a substitution trace:
  //
  //   size_of(42);
}

TEST_CASE("composed concepts") {
  CHECK(first_element(std::vector<int>{7, 8}) == 7);
  CHECK(first_element(std::string{"xy"}) == 'x');
}

TEST_CASE("concepts are ordinary compile-time predicates") {
  static_assert(std::integral<int>);
  static_assert(!std::integral<double>);
  static_assert(std::floating_point<double>);
  static_assert(std::convertible_to<const char*, std::string_view>);

  // ...and so are the ones you wrote.
  static_assert(Sized<std::string>);
  static_assert(!Sized<int>);
  static_assert(SizedSequence<std::vector<int>>);
  static_assert(!SizedSequence<int>);
  CHECK(true);
}
