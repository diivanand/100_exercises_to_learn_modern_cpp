// =============================================================================
//  07.03 -- views::filter and views::transform
// =============================================================================
//
//  A VIEW is a range that does not own its elements and is cheap to copy and
//  to move. `std::views::filter` and `std::views::transform` are the two you
//  will use constantly:
//
//      auto result = values
//                  | std::views::filter([](int n) { return n % 2 == 0; })
//                  | std::views::transform([](int n) { return n * n; });
//
//  Read left to right, in the order things happen. Compare with the same thing
//  written as algorithms -- two temporary vectors and two output iterators --
//  or as a loop, where the filter and the transform are tangled together in
//  one body.
//
//  `|` is `operator|` overloaded for range adaptors. `views::filter(pred)` on
//  its own is a "range adaptor closure": an object waiting for a range.
//
//  What a view is NOT: a container. It has no `push_back`, it may have no
//  `size()`, and it does not store results (07.04). To get a container back,
//  you have to ask (07.06).
//
//  A cost worth knowing: `filter_view`'s `begin()` has to find the first
//  matching element, so it is O(n) -- and, since it caches the result, calling
//  `begin()` on a const filter_view is not allowed. That is why filter views
//  are usually consumed once, immediately.
//
//  TASK
//    Build the pipelines the tests describe.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 07_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <ranges>
#include <string>
#include <vector>

struct Order {
  std::string customer;
  int total_cents = 0;
  bool paid = false;
};

// TODO: return a view over the paid orders. The return type is deliberately
// `auto` -- view types are unspellable in practice, which is what `auto` is
// for (01.01).
//
// Careful: the view refers to `orders`, so it must not outlive it (07.08).
auto paid_orders(const std::vector<Order>& orders) {
  return orders;
}

// TODO: return a view of the customer names of the paid orders.
auto paid_customers(const std::vector<Order>& orders) {
  return orders;
}

// TODO: sum the totals of the paid orders. A view is a range, so a range-for
// over it works -- no vector required.
int paid_total(const std::vector<Order>& orders) {
  return 0;
}

// TODO: return a view of the squares of the even numbers, doubled. Chain three
// adaptors and notice that the intermediate results never exist.
auto even_squares_doubled(const std::vector<int>& values) {
  return values;
}

namespace {

std::vector<Order> sample() {
  return {{"ada", 1000, true},
          {"alan", 250, false},
          {"grace", 4000, true},
          {"edsger", 75, false}};
}

} // namespace

TEST_CASE("filter selects without copying") {
  const auto orders = sample();
  int count = 0;
  for (const Order& order : paid_orders(orders)) {
    CHECK(order.paid);
    ++count;
  }
  CHECK(count == 2);
}

TEST_CASE("transform maps") {
  const auto orders = sample();
  std::vector<std::string> names;
  for (const auto& name : paid_customers(orders)) {
    names.push_back(name);
  }
  CHECK(names == std::vector<std::string>{"ada", "grace"});
}

TEST_CASE("a pipeline is still just a range") {
  const auto orders = sample();
  CHECK(paid_total(orders) == 5000);
  CHECK(paid_total({}) == 0);
}

TEST_CASE("three adaptors, no intermediate containers") {
  const std::vector<int> values = {1, 2, 3, 4, 5, 6};
  std::vector<int> result;
  for (const int value : even_squares_doubled(values)) {
    result.push_back(value);
  }
  // 2 -> 4 -> 8, 4 -> 16 -> 32, 6 -> 36 -> 72
  CHECK(result == std::vector<int>{8, 32, 72});
}
