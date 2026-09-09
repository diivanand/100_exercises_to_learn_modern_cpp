// =============================================================================
//  08.07 -- requires clauses and requires expressions
// =============================================================================
//
//  Two different things share the keyword, and confusing them is the usual
//  stumbling block.
//
//  A REQUIRES CLAUSE constrains a template. It takes a compile-time boolean:
//
//      template <typename T>
//      requires std::integral<T>
//      void f(T value);
//
//  A REQUIRES EXPRESSION produces a compile-time boolean by asking whether
//  some code would compile:
//
//      requires(T a, T b) {
//        a + b;                                  // valid expression
//        typename T::value_type;                 // valid type
//        { a.size() } -> std::convertible_to<std::size_t>;  // valid, and
//                                                           // convertible
//        requires sizeof(T) <= 8;                // a nested requirement:
//                                                // this bool must be true
//      }
//
//  Nothing inside is evaluated -- it is all a compilation question. Which is
//  why you see them together, and why `requires requires` is legal (if ugly):
//
//      template <typename T>
//      requires requires(T a) { a.begin(); }     // clause, then expression
//      void g(T value);
//
//  Prefer naming the expression as a concept (08.08) rather than writing
//  `requires requires`.
//
//  There are four places to put a constraint, all equivalent:
//
//      template <std::integral T> void f(T);              terse
//      template <typename T> requires std::integral<T>    clause before
//      template <typename T> void f(T) requires ...       trailing clause
//      void f(std::integral auto value);                  constrained auto
//
//  TASK
//    Write the requires expressions the tests describe.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 08_07
//
// =============================================================================

#include <doctest/doctest.h>

#include <concepts>
#include <cstddef>
#include <list>
#include <string>
#include <vector>

// TODO: true if `T` supports `a + b` and the result is convertible to T.
template <typename T>
concept Addable = false;

// TODO: true if T has a nested `value_type`, a `begin()` and an `end()`.
template <typename T>
concept Iterable = false;

// TODO: true if T is Iterable AND its elements are Addable. Reach the element
// type through `typename T::value_type`.
template <typename T>
concept SummableContainer = false;

// TODO: true if T can be streamed into a std::string with `+=`, and is small
// enough to pass by value (8 bytes or fewer). The size test is a NESTED
// requirement -- `requires sizeof(T) <= 8;` inside the expression.
template <typename T>
concept SmallAppendable = false;

// TODO: constrain this to SummableContainer, with a trailing requires clause.
template <typename T>
auto total(const T& container) {
  typename T::value_type sum{};
  for (const auto& value : container) {
    sum += value;
  }
  return sum;
}

TEST_CASE("Addable") {
  static_assert(Addable<int>);
  static_assert(Addable<double>);
  static_assert(Addable<std::string>);
  static_assert(!Addable<std::vector<int>>);
  CHECK(true);
}

TEST_CASE("Iterable") {
  static_assert(Iterable<std::vector<int>>);
  static_assert(Iterable<std::string>);
  static_assert(Iterable<std::list<double>>);
  static_assert(!Iterable<int>);
  CHECK(true);
}

TEST_CASE("a concept built from other concepts") {
  static_assert(SummableContainer<std::vector<int>>);
  static_assert(SummableContainer<std::vector<std::string>>);
  static_assert(SummableContainer<std::list<double>>);

  // A vector of vectors is iterable, but its elements are not addable.
  static_assert(!SummableContainer<std::vector<std::vector<int>>>);
  static_assert(!SummableContainer<int>);
  CHECK(true);
}

TEST_CASE("a nested requirement") {
  static_assert(SmallAppendable<char>);
  static_assert(SmallAppendable<int>);

  // A std::string can be appended to a string, but is not small.
  static_assert(!SmallAppendable<std::string>);
  static_assert(!SmallAppendable<std::vector<int>>);
  CHECK(true);
}

TEST_CASE("the constrained function") {
  CHECK(total(std::vector<int>{1, 2, 3}) == 6);
  CHECK(total(std::vector<double>{0.5, 0.5}) == doctest::Approx(1.0));
  CHECK(total(std::vector<std::string>{"a", "b"}) == "ab");
  CHECK(total(std::list<int>{4, 5}) == 9);

  // Not a SummableContainer, so this is rejected at the call with a message
  // naming the constraint:
  //
  //   total(42);
}
