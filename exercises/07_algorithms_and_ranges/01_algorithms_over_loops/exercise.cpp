// =============================================================================
//  07.01 -- Prefer algorithms to raw loops
// =============================================================================
//
//  A named algorithm says WHAT is happening; a loop says HOW and leaves the
//  reader to infer the what. That is the whole argument, and it is enough
//  (Core Guidelines SL.con, and Sean Parent's "No Raw Loops").
//
//  The practical benefits: fewer off-by-one and iterator bugs, no chance of
//  the loop and the condition drifting apart, and a name a reader can look up.
//
//  C++20 adds the `std::ranges::` versions, which take a container directly
//  instead of a pair of iterators:
//
//      std::sort(v.begin(), v.end());     // C++98
//      std::ranges::sort(v);              // C++20
//
//  Use the ranges versions. They are shorter, they cannot be given mismatched
//  iterators from two different containers, and they support projections
//  (07.02).
//
//  The ones worth memorising:
//
//      find / find_if                   is it there? (C++23 adds `contains`)
//      count / count_if                 how many?
//      all_of / any_of / none_of        a question about every element
//      transform                        map
//      copy_if / remove_copy_if         filter
//      accumulate / reduce              fold
//      sort / stable_sort / partial_sort
//      min_element / max_element / minmax_element
//      lower_bound / equal_range        binary search, on sorted input
//
//  TASK
//    Replace each loop with the algorithm that names what it does. Two of them
//    are also subtly wrong -- which is the other half of the argument.
//
//  RUN IT
//    ./mcpp test 07_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <string>
#include <vector>

// TODO: std::ranges::any_of
bool contains_negative(const std::vector<int>& values) {
  for (const int value : values) {
    if (value < 0) {
      return true;
    }
  }
  return false;
}

// TODO: std::ranges::count_if
int count_long_words(const std::vector<std::string>& words, std::size_t minimum) {
  int count = 0;
  for (const auto& word : words) {
    if (word.size() >= minimum) {
      ++count;
    }
  }
  return count;
}

// TODO: std::accumulate (there is no ranges::accumulate in C++20 -- that is
// std::ranges::fold_left in C++23 -- so use the iterator form here).
//
// Watch the type of the initial value: `std::accumulate(v.begin(), v.end(), 0)`
// over a vector<double> accumulates in an INT and silently truncates. Give the
// initial value the type you want the sum to have.
double mean(const std::vector<double>& values) {
  if (values.empty()) {
    return 0.0;
  }
  double total = 0.0;
  for (const double value : values) {
    total += value;
  }
  return total / static_cast<double>(values.size());
}

// TODO: std::ranges::max_element -- and remember it returns an ITERATOR, which
// is end() for an empty range.
std::string longest(const std::vector<std::string>& words) {
  if (words.empty()) {
    return {};
  }
  std::string best = words.front();
  for (const auto& word : words) {
    if (word.size() >= best.size()) {
      best = word;
    }
  }
  return best;
}

// TODO: std::ranges::transform into a reserved vector -- or, once you have met
// views (07.03), a pipeline. Either is fine; write the transform version here.
std::vector<int> lengths(const std::vector<std::string>& words) {
  std::vector<int> result;
  for (const auto& word : words) {
    result.push_back(static_cast<int>(word.size()));
  }
  return result;
}

// TODO: std::ranges::sort with a comparator, then std::ranges::equal_range or
// std::ranges::lower_bound. The point of this one is that a binary search over
// a sorted range is O(log n) and a hand-written scan is not -- and that the
// standard has the tricky version already written.
std::vector<int> values_at_least(std::vector<int> values, int threshold) {
  std::vector<int> result;
  for (const int value : values) {
    if (value > threshold) {
      result.push_back(value);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
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
  // Ties go to the first: max_element returns the first of equal maxima.
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
