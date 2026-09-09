// =============================================================================
//  07.08 -- Views and lifetimes
// =============================================================================
//
//  Everything a view is good at follows from one fact: it does not own its
//  elements. So does everything that can go wrong with one.
//
//  THE RULE: a view must not outlive the range it refers to.
//
//  The three shapes of the bug:
//
//   1. RETURNING A VIEW OF A LOCAL.
//          auto f() { std::vector<int> v = ...; return v | views::filter(p); }
//      The vector dies at the return; the view is left pointing at nothing.
//
//   2. A VIEW OF A TEMPORARY.
//          auto view = make_vector() | views::transform(f);
//      The temporary vector is destroyed at the end of that statement.
//      C++20's ranges library tries hard to stop this -- `borrowed_range`,
//      `owning_view`, `dangling` -- but it cannot catch every case.
//
//   3. A VIEW THAT OUTLIVES A MODIFICATION. A view over a vector is
//      invalidated by anything that reallocates it (06.02), exactly as an
//      iterator would be.
//
//  What the library gives you:
//   * `std::ranges::borrowed_range` -- a range whose iterators stay valid even
//     if the range object itself goes away (a span, a string_view, a ref_view).
//   * `std::ranges::dangling` -- what a ranges algorithm returns instead of an
//     iterator when you pass it an rvalue non-borrowed range, so the mistake
//     is a compile error at the point of use.
//   * `views::all` on an rvalue container produces an `owning_view`, which
//     takes the container with it -- the safe way to build a pipeline over a
//     temporary.
//
//  TASK
//    Fix the three dangling functions.
//
//  NOTE  This exercise starts as a compile error, and run it under
//        `cmake --preset asan` once it builds -- some of these bugs produce
//        plausible answers.
//
//  RUN IT
//    ./mcpp test 07_08
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <ranges>
#include <span>
#include <string_view>
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

std::vector<int> load() {
  return {1, 2, 3, 4, 5, 6};
}

} // namespace

// TODO: this returns a view into a vector that is destroyed on the way out.
// Return a materialised std::vector<int> instead -- the caller is keeping it,
// so it must own it.
auto even_values_broken() {
  const std::vector<int> values = load();
  return values | std::views::filter([](int n) { return n % 2 == 0; });
}

// A pipeline over a temporary.
//
// TODO: `load()` returns a prvalue, so the vector dies at the end of the
// statement that builds the view. Either keep the vector in a named variable,
// or let the pipeline own it -- `std::views::all(load())` produces an
// owning_view that carries the vector along.
std::vector<int> doubled_from_source() {
  auto view = load() | std::views::transform([](int n) { return n * 2; });
  return to_vector(view);
}

// Finds the first value above `threshold` and returns it.
//
// TODO: `std::ranges::find_if` over an rvalue container returns
// `std::ranges::dangling` rather than an iterator -- the library refusing to
// hand you something unusable. Give the range a name so it outlives the call.
int first_above(int threshold) {
  const auto it =
      std::ranges::find_if(load(), [threshold](int n) { return n > threshold; });
  return *it;
}

TEST_CASE("even_values_broken returns something the caller can keep") {
  const auto values = even_values_broken();
  CHECK(to_vector(values) == std::vector<int>{2, 4, 6});
}

TEST_CASE("a pipeline over a temporary must own it") {
  CHECK(doubled_from_source() == std::vector<int>{2, 4, 6, 8, 10, 12});
}

TEST_CASE("find_if over a temporary range") {
  CHECK(first_above(3) == 4);
  CHECK(first_above(0) == 1);
}

TEST_CASE("a view over a named container is fine, and is the normal case") {
  const std::vector<int> values = load();
  // `auto`, not `const auto`: a const filter_view is not even a range, because
  // begin() has to find (and cache) the first match and so cannot be const.
  auto odds = values | std::views::filter([](int n) { return n % 2 == 1; });
  // `values` outlives `odds`, so there is nothing wrong here at all.
  CHECK(to_vector(odds) == std::vector<int>{1, 3, 5});
}

TEST_CASE("some views are borrowed ranges and some are not") {
  // A span or a string_view can be copied out of the function that made it;
  // the elements belong to somebody else either way.
  static_assert(std::ranges::borrowed_range<std::string_view>);
  static_assert(std::ranges::borrowed_range<std::span<int>>);

  // A vector is not: its iterators die with it.
  static_assert(!std::ranges::borrowed_range<std::vector<int>>);
  CHECK(true);
}
