// Solution -- 07.01 Prefer algorithms to raw loops
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <string>
#include <vector>

bool contains_negative(const std::vector<int>& values) {
  return std::ranges::any_of(values, [](int value) { return value < 0; });
}

int count_long_words(const std::vector<std::string>& words, std::size_t minimum) {
  return static_cast<int>(std::ranges::count_if(
      words, [minimum](const std::string& word) { return word.size() >= minimum; }));
}

double mean(const std::vector<double>& values) {
  if (values.empty()) {
    return 0.0;
  }
  // The 0.0 is load-bearing: `0` would accumulate in int.
  const double total = std::accumulate(values.begin(), values.end(), 0.0);
  return total / static_cast<double>(values.size());
}

std::string longest(const std::vector<std::string>& words) {
  const auto it =
      std::ranges::max_element(words, [](const std::string& a, const std::string& b) {
        return a.size() < b.size();
      });
  return it == words.end() ? std::string{} : *it;
}

std::vector<int> lengths(const std::vector<std::string>& words) {
  std::vector<int> result;
  result.reserve(words.size());
  std::ranges::transform(words, std::back_inserter(result), [](const std::string& word) {
    return static_cast<int>(word.size());
  });
  return result;
}

std::vector<int> values_at_least(std::vector<int> values, int threshold) {
  // Sort once, then take the tail from the first element not below the
  // threshold. Binary search rather than a scan.
  std::ranges::sort(values);
  const auto first = std::ranges::lower_bound(values, threshold);
  return {first, values.end()};
}

TEST_CASE("contains_negative") {
  CHECK(contains_negative({1, -2, 3}));
  CHECK_FALSE(contains_negative({1, 2, 3}));
  CHECK_FALSE(contains_negative({}));
}

TEST_CASE("count_long_words") {
  CHECK(count_long_words({"a", "bb", "ccc"}, 2) == 2);
  CHECK(count_long_words({}, 1) == 0);
}

TEST_CASE("mean does not truncate") {
  CHECK(mean({1.0, 2.0}) == doctest::Approx(1.5));
  CHECK(mean({0.5, 0.5, 0.5}) == doctest::Approx(0.5));
  CHECK(mean({}) == doctest::Approx(0.0));
}

TEST_CASE("longest") {
  CHECK(longest({"a", "elephant", "cat"}) == "elephant");
  CHECK(longest({}).empty());
  CHECK(longest({"aa", "bb"}) == "aa");
}

TEST_CASE("lengths") {
  CHECK(lengths({"a", "bb", "ccc"}) == std::vector<int>{1, 2, 3});
  CHECK(lengths({}).empty());
}

TEST_CASE("values_at_least returns a sorted result") {
  CHECK(values_at_least({5, 1, 9, 3, 7}, 5) == std::vector<int>{5, 7, 9});
  CHECK(values_at_least({1, 2}, 10).empty());
  CHECK(values_at_least({}, 0).empty());
}
