// Solution -- 05.04 std::optional
#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

std::optional<std::size_t> find_index(const std::vector<int>& values, int target) {
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (values[i] == target) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<int> parse_int(std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }
  int value = 0;
  bool negative = false;
  std::size_t start = 0;
  if (text[0] == '-') {
    if (text.size() == 1) {
      return std::nullopt;
    }
    negative = true;
    start = 1;
  }
  for (std::size_t i = start; i < text.size(); ++i) {
    if (text[i] < '0' || text[i] > '9') {
      return std::nullopt;
    }
    value = value * 10 + (text[i] - '0');
  }
  return negative ? -value : value;
}

std::optional<int> first_valid(const std::vector<std::string>& texts) {
  for (const auto& text : texts) {
    if (const std::optional<int> parsed = parse_int(text)) {
      return parsed;
    }
  }
  return std::nullopt;
}

TEST_CASE("find_index says nothing when there is nothing to say") {
  const std::vector<int> values = {10, 20, 30};

  CHECK(find_index(values, 20) == 1);
  CHECK(find_index(values, 10).has_value());
  CHECK_FALSE(find_index(values, 99).has_value());

  CHECK(find_index(values, 99).value_or(0) == 0);
}

TEST_CASE("parse_int returns the value or nothing") {
  CHECK(parse_int("42") == 42);
  CHECK(parse_int("-7") == -7);
  CHECK(parse_int("0") == 0);

  CHECK_FALSE(parse_int("").has_value());
  CHECK_FALSE(parse_int("-").has_value());
  CHECK_FALSE(parse_int("12a").has_value());
  CHECK_FALSE(parse_int("hello").has_value());
}

TEST_CASE("value() throws where operator* would be undefined") {
  const std::optional<int> nothing = parse_int("nope");
  CHECK_THROWS_AS((void)nothing.value(), std::bad_optional_access);
  CHECK(nothing.value_or(-1) == -1);
}

TEST_CASE("first_valid finds the first parseable entry") {
  CHECK(first_valid({"x", "y", "3", "4"}) == 3);
  CHECK(first_valid({"1"}) == 1);
  CHECK_FALSE(first_valid({"a", "b"}).has_value());
  CHECK_FALSE(first_valid({}).has_value());
}

TEST_CASE("an optional costs a bool, not an allocation") {
  static_assert(sizeof(std::optional<int>) <= 2 * sizeof(int));
  static_assert(std::is_trivially_destructible_v<std::optional<int>>);
  CHECK(true);
}
