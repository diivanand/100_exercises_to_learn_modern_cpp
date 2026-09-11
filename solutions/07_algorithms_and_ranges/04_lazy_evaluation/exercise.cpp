// Solution -- 07.04 Views are lazy
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

auto pipeline(const std::vector<int>& values) {
  return values | std::views::filter(is_even) | std::views::transform(expensive_square);
}

std::optional<int> first_matching(const std::vector<int>& values, int threshold) {
  // Returning from inside the loop is what makes this short-circuit: nothing
  // beyond the first hit is ever computed.
  for (const int value : pipeline(values)) {
    if (value > threshold) {
      return value;
    }
  }
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
  CHECK(transform_calls == 2);
  CHECK(predicate_calls == 6);
}

TEST_CASE("first_matching stops at the first hit") {
  reset_counts();
  const std::vector<int> values = {1, 2, 3, 4, 5, 6, 7, 8};

  const auto found = first_matching(values, 10);
  REQUIRE(found.has_value());
  CHECK(*found == 16);

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
  CHECK(transform_calls == 4);
}
