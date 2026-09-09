// Solution -- 06.06 map, unordered_map, and operator[]
#include <doctest/doctest.h>

#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using Counts = std::unordered_map<std::string, int>;

// Takes the map by const reference now -- which is only possible because it no
// longer modifies it. The signature documents the fix.
int count_of(const Counts& counts, const std::string& word) {
  const auto it = counts.find(word);
  return it == counts.end() ? 0 : it->second;
}

Counts count_words(const std::vector<std::string>& words) {
  Counts counts;
  for (const auto& word : words) {
    ++counts[word];
  }
  return counts;
}

std::vector<std::pair<std::string, int>> report(const Counts& counts) {
  // A std::map is sorted by construction, so the ordering is not something the
  // caller has to remember to do.
  const std::map<std::string, int> ordered{counts.begin(), counts.end()};

  std::vector<std::pair<std::string, int>> result;
  result.reserve(ordered.size());
  for (const auto& [word, count] : ordered) {
    result.emplace_back(word, count);
  }
  return result;
}

TEST_CASE("reading a count must not create an entry") {
  Counts counts = count_words({"a", "b", "a"});
  CHECK(counts.size() == 2);

  CHECK(count_of(counts, "a") == 2);
  CHECK(count_of(counts, "zzz") == 0);

  CHECK(counts.size() == 2);
  CHECK_FALSE(counts.contains("zzz"));
}

TEST_CASE("counting with operator[] is the right use of it") {
  const Counts counts = count_words({"x", "y", "x", "x"});
  CHECK(counts.at("x") == 3);
  CHECK(counts.at("y") == 1);
  CHECK(counts.size() == 2);
}

TEST_CASE("the report is alphabetical") {
  const Counts counts = count_words({"pear", "apple", "fig", "apple"});
  const auto rows = report(counts);

  REQUIRE(rows.size() == 3);
  CHECK(rows[0].first == "apple");
  CHECK(rows[0].second == 2);
  CHECK(rows[1].first == "fig");
  CHECK(rows[2].first == "pear");
}

TEST_CASE("at() throws for a missing key, on both containers") {
  const Counts counts = count_words({"a"});
  CHECK_THROWS_AS((void)counts.at("missing"), std::out_of_range);

  const std::map<std::string, int> ordered = {{"a", 1}};
  CHECK_THROWS_AS((void)ordered.at("missing"), std::out_of_range);
}
