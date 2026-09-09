// Solution -- 06.08 std::tuple
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

struct Employee {
  std::string surname;
  std::string forename;
  int age = 0;
};

bool comes_before(const Employee& lhs, const Employee& rhs) {
  // std::tie makes a tuple of references -- no strings are copied -- and
  // tuple's comparison is lexicographic, which is exactly the ordering wanted.
  return std::tie(lhs.surname, lhs.forename, lhs.age) <
         std::tie(rhs.surname, rhs.forename, rhs.age);
}

std::tuple<int, int, std::size_t> summarise(const std::vector<int>& values) {
  if (values.empty()) {
    return {0, 0, 0};
  }
  const auto [smallest, largest] = std::minmax_element(values.begin(), values.end());
  return {*smallest, *largest, values.size()};
}

template <typename F, typename... Args>
auto call_with(F&& f, const std::tuple<Args...>& arguments) {
  return std::apply(std::forward<F>(f), arguments);
}

template <typename A, typename B>
std::vector<std::pair<A, B>> zip(const std::vector<A>& first,
                                 const std::vector<B>& second) {
  const std::size_t count = std::min(first.size(), second.size());
  std::vector<std::pair<A, B>> result;
  result.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    result.emplace_back(first[i], second[i]);
  }
  return result;
}

TEST_CASE("std::tie gives lexicographic ordering for free") {
  const Employee a{"Lovelace", "Ada", 36};
  const Employee b{"Lovelace", "Byron", 20};
  const Employee c{"Turing", "Alan", 41};

  CHECK(comes_before(a, b));
  CHECK(comes_before(b, c));
  CHECK_FALSE(comes_before(c, a));
  CHECK_FALSE(comes_before(a, a));

  std::vector<Employee> employees = {c, b, a};
  std::sort(employees.begin(), employees.end(), comes_before);
  CHECK(employees[0].forename == "Ada");
  CHECK(employees[2].surname == "Turing");
}

TEST_CASE("a tuple carries several values home") {
  const auto [smallest, largest, count] = summarise({3, 1, 4, 1, 5});
  CHECK(smallest == 1);
  CHECK(largest == 5);
  CHECK(count == 5);

  const auto empty = summarise({});
  CHECK(std::get<2>(empty) == 0);
}

TEST_CASE("std::apply spreads a tuple into arguments") {
  const auto add = [](int a, int b, int c) { return a + b + c; };
  CHECK(call_with(add, std::tuple{1, 2, 3}) == 6);

  const auto join = [](const std::string& a, const std::string& b) { return a + b; };
  CHECK(call_with(join, std::tuple{std::string{"mod"}, std::string{"ern"}}) == "modern");
}

TEST_CASE("zip stops at the shorter input") {
  const std::vector<int> numbers = {1, 2, 3};
  const std::vector<std::string> names = {"one", "two"};

  const auto zipped = zip(numbers, names);
  REQUIRE(zipped.size() == 2);
  CHECK(zipped[0].first == 1);
  CHECK(zipped[0].second == "one");
  CHECK(zipped[1].first == 2);

  CHECK(zip(std::vector<int>{}, names).empty());
}
