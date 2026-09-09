// =============================================================================
//  08.01 -- Function templates
// =============================================================================
//
//  A template is a pattern the compiler stamps out once per set of types it is
//  used with. Nothing exists until it is instantiated, which is why template
//  errors arrive at the call site rather than the definition.
//
//  Deduction rules worth knowing, because they are the same ones `auto` uses
//  (01.01):
//
//      template <typename T> void f(T x);         T strips const and &
//      template <typename T> void f(T& x);        T keeps const
//      template <typename T> void f(const T& x);  the usual read-only parameter
//      template <typename T> void f(T&& x);       FORWARDING reference (08.09)
//
//  Two ways to say what a template returns:
//
//      template <typename T, typename U>
//      auto add(T a, U b) -> decltype(a + b);   trailing return type
//
//      template <typename T, typename U>
//      auto add(T a, U b);                      deduced from the return
//                                               statements (C++14)
//
//  The trailing form is needed when the return type mentions the parameters,
//  because at the point of the leading return type they are not in scope yet.
//  It also participates in overload resolution, which a deduced return type
//  does not.
//
//  `std::common_type_t<T, U>` answers "what type would `a + b` have?" for the
//  cases where you want to say it explicitly.
//
//  TASK
//    Write the four templates.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 08_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <type_traits>
#include <vector>

// TODO: return the larger of two values of the same type. Take them by const
// reference and return a const reference -- copying a std::string to compare
// it would be a waste, and the result is one of the arguments.
//
// (There is a subtlety here: returning a reference to a parameter means the
// caller must not pass a temporary and keep the result. std::max has exactly
// this hazard.)
int larger(int a, int b) {
  return a;
}

// TODO: add two values of possibly different types, returning whatever their
// sum's type is. Use a trailing return type with decltype.
int add(int a, int b) {
  return a + b;
}

// TODO: return the sum of a vector's elements, accumulated in a type wide
// enough to hold it. Make the element type a template parameter.
int sum(const std::vector<int>& values) {
  return 0;
}

// TODO: convert every element to `To`. The output element type has to be
// given explicitly, since nothing in the arguments mentions it -- put it
// FIRST in the parameter list so callers write `convert<double>(v)` and let
// the input type deduce.
std::vector<int> convert(const std::vector<int>& values) {
  return values;
}

TEST_CASE("larger works for anything with <") {
  CHECK(larger(1, 2) == 2);
  CHECK(larger(2.5, 1.5) == doctest::Approx(2.5));

  const std::string a = "apple";
  const std::string b = "banana";
  CHECK(larger(a, b) == "banana");
  // The result refers to one of the arguments rather than a copy.
  CHECK(&larger(a, b) == &b);
}

TEST_CASE("add deduces a sensible result type") {
  CHECK(add(1, 2) == 3);
  CHECK(add(1, 2.5) == doctest::Approx(3.5));

  static_assert(std::is_same_v<decltype(add(1, 2)), int>);
  static_assert(std::is_same_v<decltype(add(1, 2.5)), double>);
  static_assert(std::is_same_v<decltype(add(1.0F, 2.0)), double>);

  CHECK(add(std::string{"mod"}, std::string{"ern"}) == "modern");
}

TEST_CASE("sum works for any numeric element type") {
  CHECK(sum(std::vector<int>{1, 2, 3}) == 6);
  CHECK(sum(std::vector<double>{0.5, 0.25}) == doctest::Approx(0.75));
  CHECK(sum(std::vector<int>{}) == 0);
}

TEST_CASE("convert takes its result type explicitly") {
  const std::vector<int> integers = {1, 2, 3};
  const std::vector<double> doubles = convert<double>(integers);

  CHECK(doubles.size() == 3);
  CHECK(doubles[1] == doctest::Approx(2.0));

  const std::vector<long> longs = convert<long>(integers);
  CHECK(longs[2] == 3L);
}
