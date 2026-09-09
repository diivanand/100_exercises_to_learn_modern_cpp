// Solution -- 09.01 Lambdas
#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>

inline const auto square = [](int value) { return value * value; };

inline const auto is_long = [](const std::string& text) { return text.size() > 3; };

void sort_by_length(std::vector<std::string>& words) {
  std::ranges::sort(words, [](const std::string& a, const std::string& b) {
    return a.size() < b.size();
  });
}

const std::string& longer(const std::string& a, const std::string& b) {
  // Without `-> const std::string&` the deduced return type is std::string,
  // and the lambda would hand back a copy of a local reference's target.
  const auto pick = [](const std::string& x, const std::string& y) -> const std::string& {
    return x.size() >= y.size() ? x : y;
  };
  return pick(a, b);
}

template <typename Predicate>
int count_matching(const std::vector<int>& values, Predicate predicate) {
  return static_cast<int>(std::ranges::count_if(values, predicate));
}

TEST_CASE("a lambda is a callable object") {
  CHECK(square(4) == 16);
  CHECK(square(-3) == 9);

  static_assert(!std::is_same_v<decltype(square), decltype(is_long)>);
  CHECK(true);
}

TEST_CASE("a lambda as a predicate") {
  CHECK(is_long(std::string{"abcd"}));
  CHECK_FALSE(is_long(std::string{"abc"}));
  CHECK_FALSE(is_long(std::string{}));
}

TEST_CASE("a lambda as a comparator") {
  std::vector<std::string> words = {"ccc", "a", "bb"};
  sort_by_length(words);
  CHECK(words == std::vector<std::string>{"a", "bb", "ccc"});
}

TEST_CASE("returning a reference needs an explicit return type") {
  const std::string a = "short";
  const std::string b = "much longer";

  CHECK(longer(a, b) == "much longer");
  CHECK(&longer(a, b) == &b);
}

TEST_CASE("taking a callable as a template parameter") {
  const std::vector<int> values = {1, -2, 3, -4, 5};

  CHECK(count_matching(values, [](int n) { return n > 0; }) == 3);
  CHECK(count_matching(values, [](int n) { return n % 2 == 0; }) == 2);
  CHECK(count_matching(values, [](int) { return false; }) == 0);
}
