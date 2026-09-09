// =============================================================================
//  09.03 -- Generic and templated lambdas
// =============================================================================
//
//  `auto` in a lambda's parameter list makes `operator()` a template:
//
//      auto print = [](const auto& value) { ... };   // C++14
//
//  That is enough most of the time. When it is not -- because you need to name
//  the type, constrain it, or get at a parameter pack -- C++20 lets a lambda
//  have an explicit template parameter list:
//
//      auto f = []<typename T>(const std::vector<T>& v) { ... };
//      auto g = []<typename... Ts>(Ts&&... args) { ... };
//
//  And, since a lambda parameter can be a constrained `auto` (08.07):
//
//      auto h = [](std::integral auto value) { ... };
//
//  Where generic lambdas shine: as visitors (05.06), as comparators that work
//  for several containers, and anywhere you would otherwise write a small
//  function template just to pass it somewhere.
//
//  TASK
//    Write the five lambdas.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 09_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <concepts>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// TODO: a generic lambda returning the larger of two values.
inline const auto larger = 0;

// TODO: a lambda with an explicit template parameter list that returns the
// size of a std::vector<T> -- and accepts nothing else. `[]<typename T>(...)`.
inline const auto vector_size = 0;

// TODO: a lambda constrained with `std::integral auto`, doubling its argument.
// Passing a double should not compile.
inline const auto double_it = 0;

// TODO: a variadic generic lambda that returns how many arguments it was
// given. `[]<typename... Ts>(const Ts&...)`.
inline const auto arity = 0;

// TODO: a generic lambda that sums the values of any map-like container, using
// a structured binding in the loop.
inline const auto sum_values = 0;

TEST_CASE("one lambda, several types") {
  CHECK(larger(1, 2) == 2);
  CHECK(larger(2.5, 1.5) == doctest::Approx(2.5));
  CHECK(larger(std::string{"a"}, std::string{"b"}) == "b");
}

TEST_CASE("an explicit template parameter list") {
  CHECK(vector_size(std::vector<int>{1, 2, 3}) == 3);
  CHECK(vector_size(std::vector<std::string>{}) == 0);

  // Not a vector, so this does not compile:
  //
  //   vector_size(std::string{"abc"});
}

TEST_CASE("a constrained parameter") {
  CHECK(double_it(21) == 42);
  CHECK(double_it(short{3}) == 6);

  // Rejected by the constraint:
  //
  //   double_it(1.5);
  static_assert(std::is_invocable_v<decltype(double_it), int>);
  static_assert(!std::is_invocable_v<decltype(double_it), double>);
}

TEST_CASE("a variadic lambda") {
  CHECK(arity() == 0);
  CHECK(arity(1) == 1);
  CHECK(arity(1, "two", 3.0) == 3);
}

TEST_CASE("a generic lambda over any map") {
  const std::map<std::string, int> ordered = {{"a", 1}, {"b", 2}};
  CHECK(sum_values(ordered) == 3);

  const std::vector<std::pair<int, int>> pairs = {{1, 10}, {2, 20}};
  CHECK(sum_values(pairs) == 30);
}
