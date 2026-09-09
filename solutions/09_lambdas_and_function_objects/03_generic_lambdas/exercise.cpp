// Solution -- 09.03 Generic and templated lambdas
#include <doctest/doctest.h>

#include <concepts>
#include <cstddef>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

inline const auto larger = [](const auto& a, const auto& b) { return a < b ? b : a; };

inline const auto vector_size = []<typename T>(const std::vector<T>& values) {
  return values.size();
};

inline const auto double_it = [](std::integral auto value) { return value * 2; };

inline const auto arity = []<typename... Ts>(const Ts&...) { return sizeof...(Ts); };

inline const auto sum_values = [](const auto& entries) {
  // The element type is deduced, so this works for a map, an unordered_map, or
  // a vector of pairs -- anything whose elements decompose into two parts.
  auto total = decltype(entries.begin()->second){};
  for (const auto& [key, value] : entries) {
    total += value;
  }
  return total;
};

TEST_CASE("one lambda, several types") {
  CHECK(larger(1, 2) == 2);
  CHECK(larger(2.5, 1.5) == doctest::Approx(2.5));
  CHECK(larger(std::string{"a"}, std::string{"b"}) == "b");
}

TEST_CASE("an explicit template parameter list") {
  CHECK(vector_size(std::vector<int>{1, 2, 3}) == 3);
  CHECK(vector_size(std::vector<std::string>{}) == 0);
}

TEST_CASE("a constrained parameter") {
  CHECK(double_it(21) == 42);
  CHECK(double_it(short{3}) == 6);

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
