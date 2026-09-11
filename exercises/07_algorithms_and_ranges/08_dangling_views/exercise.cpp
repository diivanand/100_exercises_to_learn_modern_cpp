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
//   2. RETURNING A VIEW, WHEN A VIEW IS WHAT THE CALLER WANTS. The library
//      protects a pipeline over a genuine temporary: since a 2021 fix to C++20
//      (P2415),
//          auto view = make_vector() | views::transform(f);
//      moves the vector INTO the view (an `owning_view`), so nothing dangles.
//      That protection is keyed on the value category, and only an rvalue
//      gets it. Give the vector a name and it is an lvalue, the pipeline is a
//      `ref_view` over it, and the library assumes you will keep it alive.
//      When the function has to return a lazy view (the caller may only want
//      the first few elements), materialising is not the answer -- handing
//      the vector to the pipeline is:
//          return std::views::all(std::move(v)) | views::transform(f);
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
//     takes the container with it. `views::all(std::move(named))` is the way
//     to hand a named container over to a pipeline you are about to return.
//
//  TASK
//    Fix the three dangling functions.
//
//  NOTE  This exercise starts as a compile error. Once it builds, run it
//        under `cmake --preset asan` with
//            ASAN_OPTIONS=detect_stack_use_after_return=1
//        in the environment: the second bug reads a vector object that lived
//        on a stack frame that has gone, and without that option it can print
//        an empty range, or the right answer, by luck.
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
#include <utility>
#include <vector>

namespace {

template <std::ranges::range R>
auto to_vector(R&& range) {
  std::vector<std::ranges::range_value_t<R>> result;
  if constexpr (std::ranges::sized_range<R>) {
    result.reserve(std::ranges::size(range));
  }
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

// Returns a LAZY view of the doubled values. The caller wants a view here --
// it may take only the first few -- so materialising is not the fix.
//
// TODO: `source` is a named local, so `source | transform` is a ref_view over
// it, and `source` is destroyed on the way out. Give the vector to the
// pipeline instead: `std::views::all(std::move(source))` produces an
// owning_view that carries it along.
auto doubled_view() {
  std::vector<int> source = load();
  return source | std::views::transform([](int n) { return n * 2; });
}

std::vector<int> doubled_from_source() {
  return to_vector(doubled_view());
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

TEST_CASE("a returned view must own what it refers to") {
  CHECK(doubled_from_source() == std::vector<int>{2, 4, 6, 8, 10, 12});
  // Laziness is the point of returning a view: taking two must not touch the
  // rest.
  CHECK(to_vector(doubled_view() | std::views::take(2)) == std::vector<int>{2, 4});
}

TEST_CASE("a pipeline built directly on an rvalue owns it") {
  // This one was never a bug: `load()` is an rvalue, so views::all (which the
  // pipe operator applies for you) wraps it in an owning_view.
  auto view = load() | std::views::transform([](int n) { return n * 2; });
  CHECK(to_vector(view) == std::vector<int>{2, 4, 6, 8, 10, 12});
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
