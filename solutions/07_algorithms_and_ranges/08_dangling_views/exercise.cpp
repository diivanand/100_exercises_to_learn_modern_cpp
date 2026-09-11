// Solution -- 07.08 Views and lifetimes
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

// Materialised: the caller owns the result, and nothing points at a dead local.
std::vector<int> even_values_broken() {
  const std::vector<int> values = load();
  return to_vector(values | std::views::filter([](int n) { return n % 2 == 0; }));
}

// `source` is a named local, so `source | transform` would be a ref_view over
// an object about to be destroyed. std::move makes it an rvalue, and
// views::all on an rvalue builds an owning_view: the pipeline takes the vector
// with it and nothing is left behind to dangle. The result is still lazy.
auto doubled_view() {
  std::vector<int> source = load();
  return std::views::all(std::move(source)) |
         std::views::transform([](int n) { return n * 2; });
}

std::vector<int> doubled_from_source() {
  return to_vector(doubled_view());
}

int first_above(int threshold) {
  // Naming the range keeps it alive across the call, so find_if returns a real
  // iterator instead of std::ranges::dangling.
  const std::vector<int> values = load();
  const auto it =
      std::ranges::find_if(values, [threshold](int n) { return n > threshold; });
  return it == values.end() ? 0 : *it;
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
  CHECK(to_vector(odds) == std::vector<int>{1, 3, 5});
}

TEST_CASE("some views are borrowed ranges and some are not") {
  static_assert(std::ranges::borrowed_range<std::string_view>);
  static_assert(std::ranges::borrowed_range<std::span<int>>);
  static_assert(!std::ranges::borrowed_range<std::vector<int>>);
  CHECK(true);
}
