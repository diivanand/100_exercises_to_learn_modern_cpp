// Solution -- 07.05 Generating ranges
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <ranges>
#include <string>
#include <vector>

auto range_of(int first, int last) {
  return std::views::iota(first, last);
}

auto multiples(int step, int count) {
  // iota(1) is unbounded; `take` is what makes this terminate.
  return std::views::iota(1) | std::views::transform([step](int n) { return n * step; }) |
         std::views::take(count);
}

auto leading_below(const std::vector<int>& values, int limit) {
  return values | std::views::take_while([limit](int value) { return value < limit; });
}

auto keys_of(const std::map<std::string, int>& table) {
  return table | std::views::keys;
}

auto last_n(const std::vector<int>& values, std::size_t count) {
  // Clamp first: `values.size() - count` on unsigned types wraps to a huge
  // number when count is the larger of the two.
  const std::size_t to_drop = count >= values.size() ? 0 : values.size() - count;
  return values | std::views::drop(to_drop);
}

namespace {

template <std::ranges::range R>
std::vector<std::ranges::range_value_t<R>> collect(R&& range) {
  std::vector<std::ranges::range_value_t<R>> result;
  for (auto&& element : range) {
    result.push_back(element);
  }
  return result;
}

} // namespace

TEST_CASE("iota generates a bounded range") {
  CHECK(collect(range_of(0, 5)) == std::vector<int>{0, 1, 2, 3, 4});
  CHECK(collect(range_of(3, 6)) == std::vector<int>{3, 4, 5});
  CHECK(collect(range_of(0, 0)).empty());
}

TEST_CASE("an unbounded range is fine when something downstream stops it") {
  CHECK(collect(multiples(3, 4)) == std::vector<int>{3, 6, 9, 12});
  CHECK(collect(multiples(10, 1)) == std::vector<int>{10});
  CHECK(collect(multiples(2, 0)).empty());
}

TEST_CASE("take_while stops at the first failure; filter would not") {
  const std::vector<int> values = {1, 2, 9, 3, 4};

  CHECK(collect(leading_below(values, 5)) == std::vector<int>{1, 2});
  CHECK(collect(leading_below(values, 100)) == std::vector<int>{1, 2, 9, 3, 4});
  CHECK(collect(leading_below(values, 0)).empty());
}

TEST_CASE("keys of a map") {
  const std::map<std::string, int> table = {{"a", 1}, {"b", 2}, {"c", 3}};
  CHECK(collect(keys_of(table)) == std::vector<std::string>{"a", "b", "c"});
}

TEST_CASE("last_n") {
  const std::vector<int> values = {1, 2, 3, 4, 5};
  CHECK(collect(last_n(values, 2)) == std::vector<int>{4, 5});
  CHECK(collect(last_n(values, 5)) == std::vector<int>{1, 2, 3, 4, 5});
  CHECK(collect(last_n(values, 99)) == std::vector<int>{1, 2, 3, 4, 5});
  CHECK(collect(last_n(values, 0)).empty());
}
