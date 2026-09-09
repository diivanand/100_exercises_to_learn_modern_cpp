// Solution -- 02.01 Range-based for
#include <doctest/doctest.h>

#include <string>
#include <vector>

int sum(const std::vector<int>& values) {
  int total = 0;
  for (const int value : values) {
    total += value;
  }
  return total;
}

std::string join(const std::vector<std::string>& values, const std::string& separator) {
  std::string result;
  // The init-statement keeps `first` scoped to the loop -- C++20's addition to
  // range-for, and the tidy way to write "separator between, not after".
  for (bool first = true; const auto& value : values) {
    if (!first) {
      result += separator;
    }
    result += value;
    first = false;
  }
  return result;
}

void quantise(std::vector<int>& values, int step) {
  for (auto& value : values) {
    value = (value / step) * step;
  }
}

TEST_CASE("sum adds every element exactly once") {
  CHECK(sum({1, 2, 3, 4}) == 10);
  CHECK(sum({}) == 0);
  CHECK(sum({-5, 5}) == 0);
}

TEST_CASE("join puts the separator between values") {
  CHECK(join({"a", "b", "c"}, ", ") == "a, b, c");
  CHECK(join({"only"}, ", ") == "only");
  CHECK(join({}, ", ").empty());
}

TEST_CASE("quantise rounds down to a multiple of step") {
  std::vector<int> values = {0, 7, 13, 25};
  quantise(values, 5);
  CHECK(values == std::vector<int>{0, 5, 10, 25});
}
