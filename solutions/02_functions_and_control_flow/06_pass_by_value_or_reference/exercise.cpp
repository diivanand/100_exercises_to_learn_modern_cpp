// Solution -- 02.06 Choosing a parameter type
#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Expensive to copy, read-only -> const reference.
int sum(const std::vector<int>& values) {
  int total = 0;
  for (const int value : values) {
    total += value;
  }
  return total;
}

// Cheap to copy, read-only, and never kept -> string_view by value.
std::size_t count_vowels(std::string_view text) {
  std::size_t count = 0;
  for (const char c : text) {
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
      ++count;
    }
  }
  return count;
}

class Logger {
public:
  // A sink: by value, then moved. An lvalue argument costs one copy, an rvalue
  // costs one move, and the constructor body says which happens.
  explicit Logger(std::string prefix) : prefix_(std::move(prefix)) {}

  [[nodiscard]] std::string format(std::string_view message) const {
    return prefix_ + ": " + std::string{message};
  }

private:
  std::string prefix_;
};

struct MinMax {
  int min = 0;
  int max = 0;
};

// Returned, not written through a reference. Since C++17 the result is
// constructed directly in the caller's storage -- no copy, no move.
MinMax min_max(const std::vector<int>& values) {
  if (values.empty()) {
    return MinMax{};
  }
  MinMax result{values.front(), values.front()};
  for (const int value : values) {
    if (value < result.min) {
      result.min = value;
    }
    if (value > result.max) {
      result.max = value;
    }
  }
  return result;
}

TEST_CASE("sum reads without copying") {
  const std::vector<int> values = {1, 2, 3};
  CHECK(sum(values) == 6);
}

TEST_CASE("count_vowels accepts anything string-like") {
  CHECK(count_vowels("hello") == 2);
  CHECK(count_vowels(std::string{"queueing"}) == 5);

  constexpr std::string_view view = "modern";
  CHECK(count_vowels(view) == 2);
}

TEST_CASE("Logger takes ownership of its prefix") {
  Logger logger{"app"};
  CHECK(logger.format("started") == "app: started");

  std::string prefix = "service";
  Logger moved{std::move(prefix)};
  CHECK(moved.format("ok") == "service: ok");
}

TEST_CASE("min_max returns its result") {
  const MinMax result = min_max({3, 1, 4, 1, 5});
  CHECK(result.min == 1);
  CHECK(result.max == 5);

  const MinMax empty = min_max({});
  CHECK(empty.min == 0);
  CHECK(empty.max == 0);
}
