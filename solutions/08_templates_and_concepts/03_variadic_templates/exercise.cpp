// Solution -- 08.03 Variadic templates
#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

template <typename... Ts>
constexpr std::size_t count_of(const Ts&...) {
  return sizeof...(Ts);
}

namespace {

std::string stringify(const std::string& value) {
  return value;
}
std::string stringify(const char* value) {
  return value;
}
template <typename T>
std::string stringify(const T& value) {
  return std::to_string(value);
}

} // namespace

// The base case ends the recursion.
std::string concat() {
  return {};
}

template <typename First, typename... Rest>
std::string concat(const First& first, const Rest&... rest) {
  // One argument is consumed here; the remaining pack is expanded into the
  // next call, which is one shorter. Eventually the pack is empty and the base
  // case above is selected.
  return stringify(first) + concat(rest...);
}

template <typename T, typename... Ts>
std::vector<T> vector_of(const Ts&... values) {
  return {static_cast<T>(values)...};
}

template <typename First, typename... Rest>
constexpr bool all_same() {
  return std::conjunction_v<std::is_same<First, Rest>...>;
}

template <typename F>
void for_each_argument(F&&) {}

template <typename F, typename First, typename... Rest>
void for_each_argument(F&& f, const First& first, const Rest&... rest) {
  f(first);
  for_each_argument(std::forward<F>(f), rest...);
}

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
