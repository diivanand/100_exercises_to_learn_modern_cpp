// =============================================================================
//  06.06 -- map, unordered_map, and operator[]
// =============================================================================
//
//  Two associative containers, two different data structures:
//
//    std::map            balanced tree. Keys are SORTED. Lookup O(log n).
//                        Iteration is in key order. References to elements
//                        stay valid across inserts and erases.
//                        Needs `<` (or a comparator).
//
//    std::unordered_map  hash table. No order at all. Lookup O(1) average,
//                        O(n) worst case. Rehashing invalidates iterators
//                        (but NOT references to elements).
//                        Needs `std::hash` and `==`.
//
//  Choose unordered_map by default for lookup, and map when you need ordered
//  iteration, range queries, or a key type with an ordering but no hash.
//
//  THE operator[] TRAP, which applies to both. `m[key]` DEFAULT-CONSTRUCTS a
//  value when the key is missing, and returns a reference to it. So:
//
//      if (counts[key] > 0) { ... }     // just inserted `key` with value 0
//      const int n = m[key];            // does not compile on a const map
//
//  Reading a map should use `find`, `at` or `contains` (C++20). `operator[]`
//  is for writing.
//
//  TASK
//    Fix the two broken functions below: one uses operator[] to read, and one
//    relies on an order that unordered_map does not have. The third is
//    correct as it stands, and its comment says why.
//
//  RUN IT
//    ./mcpp test 06_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using Counts = std::unordered_map<std::string, int>;

// Returns how many times `word` was seen, or 0.
//
// TODO: this INSERTS `word` with a count of 0 every time it is asked about a
// word that is not there -- so the map grows as you read it, and `size()`
// stops meaning what you think. Use `find` (or `contains`) instead.
int count_of(Counts& counts, const std::string& word) {
  return counts[word];
}

// Counts the words. Here operator[] is exactly right: we are writing, and a
// missing key should start at zero.
Counts count_words(const std::vector<std::string>& words) {
  Counts counts;
  for (const auto& word : words) {
    ++counts[word];
  }
  return counts;
}

// Returns the words in alphabetical order with their counts.
//
// TODO: an unordered_map has no order, so this returns whatever the hash
// table's layout happens to be. Either sort the result, or -- better here --
// build the report from a std::map, which is sorted by construction.
std::vector<std::pair<std::string, int>> report(const Counts& counts) {
  std::vector<std::pair<std::string, int>> result;
  for (const auto& [word, count] : counts) {
    result.emplace_back(word, count);
  }
  return result;
}

TEST_CASE("reading a count must not create an entry") {
  Counts counts = count_words({"a", "b", "a"});
  CHECK(counts.size() == 2);

  CHECK(count_of(counts, "a") == 2);
  CHECK(count_of(counts, "zzz") == 0);

  // This is the assertion that fails with operator[]: asking about "zzz"
  // inserted it.
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
