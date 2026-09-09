// =============================================================================
//  07.07 -- Composing pipelines
// =============================================================================
//
//  A range adaptor without a range -- `views::filter(pred)` on its own -- is a
//  RANGE ADAPTOR CLOSURE: a callable object waiting for a range. Two of them
//  can be composed with `|` before either has seen any data:
//
//      auto evens_squared = std::views::filter(is_even)
//                         | std::views::transform(square);
//
//      auto a = numbers | evens_squared;
//      auto b = others  | evens_squared;
//
//  That is what makes pipelines reusable: a named, testable, first-class
//  transformation you can pass around, store, and apply to different inputs.
//  It is the closest thing C++ has to a point-free function pipeline.
//
//  The pieces you have not met yet:
//
//      views::join          flattens a range of ranges
//      views::split(delim)  splits a range into subranges
//      views::elements<N>   the Nth element of each tuple-like element
//      views::common        makes iterator and sentinel the same type, for
//                           old algorithms that require it
//
//  TASK
//    Build three reusable adaptor closures, then use them.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 07_07
//
// =============================================================================

#include <doctest/doctest.h>

#include <map>
#include <ranges>
#include <string>
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

// TODO: a reusable closure that keeps only the even values. Note that this is
// a VARIABLE, not a function: it is built once and applied many times.
inline constexpr auto evens = std::views::all;

// TODO: a reusable closure that squares each value.
inline constexpr auto squared = std::views::all;

// TODO: compose the two into a single closure, with `|`, before either has
// seen a range.
inline constexpr auto even_squares = std::views::all;

// Flattens a range of ranges into one range.
//
// TODO: views::join.
auto flatten(const std::vector<std::vector<int>>& groups) {
  return groups;
}

// Returns the values of a map whose keys start with `prefix`.
//
// TODO: filter on the key, then project to the value. A map's elements are
// pairs, so `views::values` is available -- but you need to filter first,
// which means reaching into `.first` yourself.
auto values_with_prefix(const std::map<std::string, int>& table, std::string prefix) {
  return table;
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
  // ...and the composition is the same as applying them one after the other.
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
