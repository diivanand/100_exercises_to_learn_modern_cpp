// =============================================================================
//  07.06 -- Turning a view back into a container
// =============================================================================
//
//  C++23 has `std::ranges::to<std::vector>()`, which is what you want:
//
//      auto result = values | views::filter(f) | std::ranges::to<std::vector>();
//
//  C++20 does not. Until then you write it yourself, and doing so is a good
//  way to learn what a range actually is.
//
//  The three ways, worst to best:
//
//   1. A loop with push_back. Correct, allocates repeatedly.
//   2. `std::vector<T>(view.begin(), view.end())`. Works only when the view's
//      iterator and sentinel are the same type, which for many views they are
//      NOT -- `iota_view` unbounded, `take_while_view`, `filter_view` over a
//      non-common range all have a sentinel that is a different type from the
//      iterator. `views::common` fixes that up when you need it.
//   3. A small `to_vector` helper that reserves when it can.
//
//  Knowing when to materialise is the real lesson. Materialise when:
//   * the result is traversed more than once (07.04);
//   * the underlying container is about to change or go away (07.08);
//   * you need `size()`, random access, or to return it across an API.
//
//  Otherwise keep it lazy.
//
//  TASK
//    Write `to_vector`, then use it. `std::ranges::sized_range` tells you at
//    compile time whether reserving is possible.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 07_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <ranges>
#include <string>
#include <vector>

// TODO: implement it.
//
//   * The element type is `std::ranges::range_value_t<R>`.
//   * `if constexpr (std::ranges::sized_range<R>)` lets you reserve exactly
//     when the range knows its size, and skip it when it does not -- which is
//     02.02's `if constexpr` earning its keep.
//   * Take the range by forwarding reference: a view is usually a temporary.
template <std::ranges::range R>
auto to_vector(R&& range) {
  return std::vector<int>{};
}

struct Product {
  std::string name;
  int price_cents = 0;
  bool in_stock = false;
};

// TODO: return the names of the in-stock products, cheapest first, as a real
// vector -- the caller is going to keep it, so it must not be a view into a
// parameter that is about to go away.
std::vector<std::string> affordable_names(const std::vector<Product>& products,
                                          int budget_cents) {
  return {};
}

TEST_CASE("to_vector materialises a sized range") {
  const std::vector<int> values = {1, 2, 3, 4};
  const auto doubled =
      to_vector(values | std::views::transform([](int n) { return n * 2; }));

  CHECK(doubled == std::vector<int>{2, 4, 6, 8});
  // transform_view over a vector is a sized range, so the capacity was
  // reserved exactly once.
  CHECK(doubled.capacity() == 4);
}

TEST_CASE("to_vector handles a range that does not know its size") {
  // filter_view is not a sized_range: you cannot know how many elements pass
  // without running the predicate over all of them.
  const std::vector<int> values = {1, 2, 3, 4, 5, 6};
  const auto evens =
      to_vector(values | std::views::filter([](int n) { return n % 2 == 0; }));

  CHECK(evens == std::vector<int>{2, 4, 6});
}

TEST_CASE("to_vector works on an unbounded generator, once bounded") {
  const auto first_five = to_vector(std::views::iota(1) | std::views::take(5));
  CHECK(first_five == std::vector<int>{1, 2, 3, 4, 5});
}

TEST_CASE("to_vector preserves the element type") {
  const std::vector<std::string> words = {"a", "bb"};
  const auto copy = to_vector(words);
  CHECK(copy == words);

  const auto sizes = to_vector(
      words | std::views::transform([](const std::string& w) { return w.size(); }));
  CHECK(sizes.size() == 2);
  CHECK(sizes[1] == 2);
}

TEST_CASE("affordable_names returns owned strings, cheapest first") {
  const std::vector<Product> products = {
      {"widget", 500, true},
      {"gadget", 150, true},
      {"gizmo", 900, true},
      {"doohickey", 100, false},
  };

  CHECK(affordable_names(products, 600) == std::vector<std::string>{"gadget", "widget"});
  CHECK(affordable_names(products, 100).empty());
  CHECK(affordable_names({}, 1000).empty());
}
