// Solution -- 07.06 Turning a view back into a container
#include <doctest/doctest.h>

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

template <std::ranges::range R>
auto to_vector(R&& range) {
  std::vector<std::ranges::range_value_t<R>> result;

  // Reserve only when the range can answer `size()` without doing the work.
  // A filter_view cannot, and asking would run the predicate over everything.
  if constexpr (std::ranges::sized_range<R>) {
    result.reserve(std::ranges::size(range));
  }

  for (auto&& element : range) {
    result.push_back(std::forward<decltype(element)>(element));
  }
  return result;
}

struct Product {
  std::string name;
  int price_cents = 0;
  bool in_stock = false;
};

std::vector<std::string> affordable_names(const std::vector<Product>& products,
                                          int budget_cents) {
  auto affordable = to_vector(products | std::views::filter([&](const Product& p) {
                                return p.in_stock && p.price_cents <= budget_cents;
                              }));

  std::ranges::sort(affordable, {}, &Product::price_cents);

  // Materialised, not a view: the caller keeps this, and `products` is not
  // ours to depend on.
  return to_vector(affordable | std::views::transform(&Product::name));
}

TEST_CASE("to_vector materialises a sized range") {
  const std::vector<int> values = {1, 2, 3, 4};
  const auto doubled =
      to_vector(values | std::views::transform([](int n) { return n * 2; }));

  CHECK(doubled == std::vector<int>{2, 4, 6, 8});
  CHECK(doubled.capacity() == 4);
}

TEST_CASE("to_vector handles a range that does not know its size") {
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
