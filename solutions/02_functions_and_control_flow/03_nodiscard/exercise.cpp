// Solution -- 02.03 [[nodiscard]] and friends
#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

[[nodiscard]] std::vector<int> sorted_copy(std::vector<int> values) {
  for (std::size_t i = 1; i < values.size(); ++i) {
    for (std::size_t j = i; j > 0 && values[j - 1] > values[j]; --j) {
      const int tmp = values[j - 1];
      values[j - 1] = values[j];
      values[j] = tmp;
    }
  }
  return values;
}

// Marking the type covers every function that returns it, now and later.
enum class [[nodiscard("a parse can fail; check the result")]] ParseResult {
  kOk,
  kEmpty,
  kNotANumber
};

ParseResult parse_int(std::string_view text, int& out) {
  if (text.empty()) {
    return ParseResult::kEmpty;
  }
  int value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') {
      return ParseResult::kNotANumber;
    }
    value = value * 10 + (c - '0');
  }
  out = value;
  return ParseResult::kOk;
}

std::string describe(ParseResult result, [[maybe_unused]] std::string_view input) {
  std::string message;
  switch (result) {
  case ParseResult::kOk:
    return "ok";
  case ParseResult::kEmpty:
    message = "empty: ";
    [[fallthrough]]; // deliberate: an empty input is also bad input
  case ParseResult::kNotANumber:
    message += "bad input";
    return message;
  }
  return "unreachable";
}

TEST_CASE("sorted_copy leaves the original alone") {
  const std::vector<int> original = {3, 1, 2};
  CHECK(sorted_copy(original) == std::vector<int>{1, 2, 3});
  CHECK(original == std::vector<int>{3, 1, 2});
}

TEST_CASE("parse_int reports why it failed") {
  int value = -1;
  CHECK(parse_int("123", value) == ParseResult::kOk);
  CHECK(value == 123);
  CHECK(parse_int("", value) == ParseResult::kEmpty);
  CHECK(parse_int("12a", value) == ParseResult::kNotANumber);
}

TEST_CASE("describe distinguishes the two failures") {
  CHECK(describe(ParseResult::kOk, "1") == "ok");
  CHECK(describe(ParseResult::kEmpty, "") == "empty: bad input");
  CHECK(describe(ParseResult::kNotANumber, "x") == "bad input");
}
