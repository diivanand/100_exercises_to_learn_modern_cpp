// Solution -- 07.03 views::filter and views::transform
#include <doctest/doctest.h>

#include <ranges>
#include <string>
#include <vector>

struct Order {
  std::string customer;
  int total_cents = 0;
  bool paid = false;
};

auto paid_orders(const std::vector<Order>& orders) {
  return orders | std::views::filter([](const Order& order) { return order.paid; });
}

auto paid_customers(const std::vector<Order>& orders) {
  return paid_orders(orders) |
         std::views::transform([](const Order& order) { return order.customer; });
}

int paid_total(const std::vector<Order>& orders) {
  int total = 0;
  for (const Order& order : paid_orders(orders)) {
    total += order.total_cents;
  }
  return total;
}

auto even_squares_doubled(const std::vector<int>& values) {
  return values | std::views::filter([](int value) { return value % 2 == 0; }) |
         std::views::transform([](int value) { return value * value; }) |
         std::views::transform([](int value) { return value * 2; });
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
  CHECK(result == std::vector<int>{8, 32, 72});
}
