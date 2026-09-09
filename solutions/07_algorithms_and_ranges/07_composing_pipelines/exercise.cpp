// Solution -- 07.07 Composing pipelines
#include <doctest/doctest.h>

#include <map>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace {

template <std::ranges::range R>
auto to_vector(R&& range) {
  std::vector<std::ranges::range_value_t<R>> result;
  for (auto&& element : range) {
    result.push_back(element);
  }
  return result;
}

} // namespace

inline constexpr auto evens =
    std::views::filter([](int value) { return value % 2 == 0; });

inline constexpr auto squared =
    std::views::transform([](int value) { return value * value; });

// Composed with `|` while both are still waiting for a range.
inline constexpr auto even_squares = evens | squared;

auto flatten(const std::vector<std::vector<int>>& groups) {
  return groups | std::views::join;
}

auto values_with_prefix(const std::map<std::string, int>& table, std::string prefix) {
  return table | std::views::filter([prefix = std::move(prefix)](const auto& entry) {
           return entry.first.starts_with(prefix);
         }) |
         std::views::values;
}

TEST_CASE("a closure can be applied to different ranges") {
  const std::vector<int> a = {1, 2, 3, 4};
  const std::vector<int> b = {10, 11, 12};

  CHECK(to_vector(a | evens) == std::vector<int>{2, 4});
  CHECK(to_vector(b | evens) == std::vector<int>{10, 12});
}

TEST_CASE("closures compose before they see any data") {
  const std::vector<int> values = {1, 2, 3, 4, 5, 6};

  CHECK(to_vector(values | even_squares) == std::vector<int>{4, 16, 36});
  CHECK(to_vector(values | evens | squared) == to_vector(values | even_squares));
}

TEST_CASE("join flattens") {
  const std::vector<std::vector<int>> groups = {{1, 2}, {}, {3}, {4, 5, 6}};
  CHECK(to_vector(flatten(groups)) == std::vector<int>{1, 2, 3, 4, 5, 6});
  CHECK(to_vector(flatten({})).empty());
}

TEST_CASE("filtering a map by key, projecting to the value") {
  const std::map<std::string, int> table = {
      {"net.port", 80}, {"net.timeout", 30}, {"log.level", 2}};

  CHECK(to_vector(values_with_prefix(table, "net.")) == std::vector<int>{80, 30});
  CHECK(to_vector(values_with_prefix(table, "log.")) == std::vector<int>{2});
  CHECK(to_vector(values_with_prefix(table, "zzz")).empty());
}

TEST_CASE("the whole point: one pipeline, two inputs, no duplication") {
  const std::vector<std::vector<int>> groups = {{1, 2, 3}, {4, 5, 6}};
  CHECK(to_vector(flatten(groups) | even_squares) == std::vector<int>{4, 16, 36});
}
