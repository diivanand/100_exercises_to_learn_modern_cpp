// =============================================================================
//  07.04 -- Views are lazy
// =============================================================================
//
//  A view does no work when you build it. `values | views::transform(f)`
//  calls `f` exactly zero times. The work happens when someone iterates, one
//  element at a time, and only for the elements actually reached.
//
//  Three consequences that make views worth learning:
//
//   1. NO INTERMEDIATE CONTAINERS. A five-stage pipeline over a million
//      elements allocates nothing.
//
//   2. SHORT-CIRCUITING FOR FREE. `filter | transform | take(3)` calls the
//      transform three times, not a million. The algorithm version has to
//      compute everything before it can take three.
//
//   3. INFINITE RANGES ARE FINE. `views::iota(0)` has no end; combined with
//      `take_while` or `take` it is perfectly ordinary (07.05).
//
//  The trap that follows from laziness: a view is RE-EVALUATED every time you
//  traverse it. Iterating a `transform_view` twice calls the function twice
//  per element. If the function is expensive or has side effects, materialise
//  the result once (07.06) rather than passing the view around.
//
//  TASK
//    Use the call counters to demonstrate all three properties, then fix
//    `first_matching`, which currently computes the whole pipeline.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 07_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <optional>
#include <ranges>
#include <vector>

namespace {

int transform_calls = 0;
int predicate_calls = 0;

void reset_counts() {
  transform_calls = 0;
  predicate_calls = 0;
}

int expensive_square(int value) {
  ++transform_calls;
  return value * value;
}

bool is_even(int value) {
  ++predicate_calls;
  return value % 2 == 0;
}

} // namespace

// TODO: return a view -- filter on is_even, then transform with
// expensive_square. Building it must not call either function.
auto pipeline(const std::vector<int>& values) {
  return values;
}

// Returns the first squared even number greater than `threshold`.
//
// TODO: implement it so it stops as soon as it finds one. A range-for with a
// `break`, or `views::take_while`/`ranges::find_if` over the pipeline -- any
// of those short-circuits. Building a vector first does not.
std::optional<int> first_matching(const std::vector<int>& values, int threshold) {
  return std::nullopt;
}

TEST_CASE("building a pipeline does no work") {
  reset_counts();
  const std::vector<int> values = {1, 2, 3, 4, 5, 6};

  // Not `const`: filter_view caches where its first element is, so begin()
  // is a non-const operation. That surprises everyone once.
  [[maybe_unused]] auto view = pipeline(values);

  CHECK(transform_calls == 0);
  CHECK(predicate_calls == 0);
}

TEST_CASE("work happens per element, on demand") {
  reset_counts();
  const std::vector<int> values = {1, 2, 3, 4, 5, 6};

  std::vector<int> result;
  for (const int value : pipeline(values)) {
    result.push_back(value);
  }

  CHECK(result == std::vector<int>{4, 16, 36});
  // Six elements tested, three of them squared.
  CHECK(predicate_calls == 6);
  CHECK(transform_calls == 3);
}

TEST_CASE("taking two elements does not compute the rest") {
  reset_counts();
  const std::vector<int> values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  std::vector<int> result;
  for (const int value : pipeline(values) | std::views::take(2)) {
    result.push_back(value);
  }

  CHECK(result == std::vector<int>{4, 16});
  // Only two elements were squared, out of ten.
  CHECK(transform_calls == 2);
  // The filter is asked about 1, 2, 3, 4 -- and then about 5 and 6, because
  // the iterator advances to the next match before `take` notices it is done.
  // Six questions instead of ten; laziness is not the same as clairvoyance.
  CHECK(predicate_calls == 6);
}

TEST_CASE("first_matching stops at the first hit") {
  reset_counts();
  const std::vector<int> values = {1, 2, 3, 4, 5, 6, 7, 8};

  const auto found = first_matching(values, 10);
  REQUIRE(found.has_value());
  CHECK(*found == 16);

  // 4*4 = 16 is the second squared even number, so squaring stops there.
  CHECK(transform_calls == 2);
}

TEST_CASE("first_matching can find nothing") {
  const std::vector<int> values = {1, 2, 3};
  CHECK_FALSE(first_matching(values, 1000).has_value());
  CHECK_FALSE(first_matching({}, 0).has_value());
}

TEST_CASE("traversing a view twice does the work twice") {
  reset_counts();
  const std::vector<int> values = {2, 4};
  auto view = pipeline(values);

  for ([[maybe_unused]] const int value : view) {
  }
  CHECK(transform_calls == 2);

  for ([[maybe_unused]] const int value : view) {
  }
  // This is the cost of laziness, and the reason to materialise a view whose
  // elements are expensive and needed more than once.
  CHECK(transform_calls == 4);
}
