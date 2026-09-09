// =============================================================================
//  08.03 -- Variadic templates
// =============================================================================
//
//  A PARAMETER PACK holds zero or more template arguments:
//
//      template <typename... Ts>          Ts is a pack of types
//      void f(Ts... values);              values is a pack of values
//
//  You never index a pack. You EXPAND it, with a trailing `...`:
//
//      sizeof...(Ts)          how many
//      f(values...)           pass them all on
//      g(h(values)...)        h applied to each, then all passed to g
//      (values + ...)         a fold expression (08.04)
//
//  Before fold expressions, the way to process a pack was recursion with a
//  base case:
//
//      void print() {}                                    // base
//      template <typename T, typename... Rest>
//      void print(const T& first, const Rest&... rest) {  // peel one off
//        std::cout << first;
//        print(rest...);
//      }
//
//  That pattern still matters: it is how you do anything that treats the
//  elements differently, or that needs the index. But when the operation is
//  uniform, a fold is shorter and compiles faster.
//
//  TASK
//    Write the recursive versions first -- you will replace two of them with
//    folds in the next exercise.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 08_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

// TODO: how many arguments were passed. One line, no recursion.
template <typename... Ts>
constexpr std::size_t count_of(const Ts&...) {
  return 0;
}

// TODO: concatenate everything into one string, by recursion. Anything with
// `std::to_string` or a std::string is fair game; keep it to strings and
// integers so the base case stays simple.
std::string concat() {
  return {};
}

// TODO: build a vector containing every argument, converted to T. This is a
// pack expansion inside a braced initialiser list:
//     return {static_cast<T>(values)...};
template <typename T, typename... Ts>
std::vector<T> vector_of(const Ts&...) {
  return {};
}

// TODO: return true if every argument is the same type as the first. Use
// `std::conjunction_v` or a fold; recursion also works.
template <typename First, typename... Rest>
constexpr bool all_same() {
  return false;
}

// TODO: call `f` once with each argument, in order. `(f(values), ...)` is a
// fold over the comma operator -- but write the recursive version here.
template <typename F>
void for_each_argument(F&&) {}

TEST_CASE("counting a pack") {
  static_assert(count_of() == 0);
  static_assert(count_of(1) == 1);
  static_assert(count_of(1, 2.0, "three") == 3);
  CHECK(true);
}

TEST_CASE("recursion peels one argument at a time") {
  CHECK(concat() == "");
  CHECK(concat("a") == "a");
  CHECK(concat("a", "b", "c") == "abc");
  CHECK(concat("x=", 42) == "x=42");
  CHECK(concat(1, 2, 3) == "123");
  CHECK(concat(std::string{"s"}, "-", 7) == "s-7");
}

TEST_CASE("expanding a pack into a container") {
  const auto values = vector_of<int>(1, 2, 3);
  CHECK(values == std::vector<int>{1, 2, 3});

  // Every argument is converted to the requested type.
  const auto doubles = vector_of<double>(1, 2.5, 3L);
  CHECK(doubles.size() == 3);
  CHECK(doubles[1] == doctest::Approx(2.5));

  CHECK(vector_of<int>().empty());
}

TEST_CASE("a compile-time question about a pack") {
  static_assert(all_same<int, int, int>());
  static_assert(all_same<int>());
  static_assert(!all_same<int, double>());
  static_assert(!all_same<int, int, char>());
  CHECK(true);
}

TEST_CASE("visiting each argument in order") {
  std::string log;
  for_each_argument([&log](const auto& value) { log += std::to_string(value); }, 1, 2, 3);
  CHECK(log == "123");

  int calls = 0;
  for_each_argument([&calls](int) { ++calls; });
  CHECK(calls == 0);
}
