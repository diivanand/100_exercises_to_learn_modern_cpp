// Solution -- 08.01 Function templates
#include <doctest/doctest.h>

#include <string>
#include <type_traits>
#include <vector>

// Returns a reference to one of its arguments -- which is why a caller must
// not pass a temporary and keep the result. std::max has the same hazard.
template <typename T>
const T& larger(const T& a, const T& b) {
  return a < b ? b : a;
}

// By value on purpose: this is the textbook shape for a trailing return type,
// and the arguments are usually numbers. A `const T&` version would avoid the
// copy for strings at the cost of obscuring the point.
template <typename T, typename U>
auto add(T a, U b) -> decltype(a + b) { // NOLINT(performance-unnecessary-value-param)
  return a + b;
}

template <typename T>
T sum(const std::vector<T>& values) {
  T total{};
  for (const auto& value : values) {
    total += value;
  }
  return total;
}

// `To` comes first because it cannot be deduced; `From` follows and is.
template <typename To, typename From>
std::vector<To> convert(const std::vector<From>& values) {
  std::vector<To> result;
  result.reserve(values.size());
  for (const auto& value : values) {
    result.push_back(static_cast<To>(value));
  }
  return result;
}

TEST_CASE("larger works for anything with <") {
  CHECK(larger(1, 2) == 2);
  CHECK(larger(2.5, 1.5) == doctest::Approx(2.5));

  const std::string a = "apple";
  const std::string b = "banana";
  CHECK(larger(a, b) == "banana");
  CHECK(&larger(a, b) == &b);
}

TEST_CASE("add deduces a sensible result type") {
  CHECK(add(1, 2) == 3);
  CHECK(add(1, 2.5) == doctest::Approx(3.5));

  static_assert(std::is_same_v<decltype(add(1, 2)), int>);
  static_assert(std::is_same_v<decltype(add(1, 2.5)), double>);
  static_assert(std::is_same_v<decltype(add(1.0F, 2.0)), double>);

  CHECK(add(std::string{"mod"}, std::string{"ern"}) == "modern");
}

TEST_CASE("sum works for any numeric element type") {
  CHECK(sum(std::vector<int>{1, 2, 3}) == 6);
  CHECK(sum(std::vector<double>{0.5, 0.25}) == doctest::Approx(0.75));
  CHECK(sum(std::vector<int>{}) == 0);
}

TEST_CASE("convert takes its result type explicitly") {
  const std::vector<int> integers = {1, 2, 3};
  const std::vector<double> doubles = convert<double>(integers);

  CHECK(doubles.size() == 3);
  CHECK(doubles[1] == doctest::Approx(2.0));

  const std::vector<long> longs = convert<long>(integers);
  CHECK(longs[2] == 3L);
}
