// Solution -- 08.07 requires clauses and requires expressions
#include <doctest/doctest.h>

#include <concepts>
#include <cstddef>
#include <list>
#include <string>
#include <vector>

template <typename T>
concept Addable = requires(const T& a, const T& b) {
  { a + b } -> std::convertible_to<T>;
};

template <typename T>
concept Iterable = requires(const T& value) {
  typename T::value_type;
  value.begin();
  value.end();
};

template <typename T>
concept SummableContainer = Iterable<T> && Addable<typename T::value_type>;

template <typename T>
concept SmallAppendable = requires(std::string& target, const T& value) {
  target += value;
  // A nested requirement: the expression after `requires` must itself be true,
  // rather than merely well-formed.
  requires sizeof(T) <= 8;
};

template <typename T>
auto total(const T& container)
  requires SummableContainer<T>
{
  typename T::value_type sum{};
  for (const auto& value : container) {
    sum += value;
  }
  return sum;
}

TEST_CASE("Addable") {
  static_assert(Addable<int>);
  static_assert(Addable<double>);
  static_assert(Addable<std::string>);
  static_assert(!Addable<std::vector<int>>);
  CHECK(true);
}

TEST_CASE("Iterable") {
  static_assert(Iterable<std::vector<int>>);
  static_assert(Iterable<std::string>);
  static_assert(Iterable<std::list<double>>);
  static_assert(!Iterable<int>);
  CHECK(true);
}

TEST_CASE("a concept built from other concepts") {
  static_assert(SummableContainer<std::vector<int>>);
  static_assert(SummableContainer<std::vector<std::string>>);
  static_assert(SummableContainer<std::list<double>>);

  static_assert(!SummableContainer<std::vector<std::vector<int>>>);
  static_assert(!SummableContainer<int>);
  CHECK(true);
}

TEST_CASE("a nested requirement") {
  static_assert(SmallAppendable<char>);
  static_assert(SmallAppendable<int>);
  static_assert(!SmallAppendable<std::string>);
  static_assert(!SmallAppendable<std::vector<int>>);
  CHECK(true);
}

TEST_CASE("the constrained function") {
  CHECK(total(std::vector<int>{1, 2, 3}) == 6);
  CHECK(total(std::vector<double>{0.5, 0.5}) == doctest::Approx(1.0));
  CHECK(total(std::vector<std::string>{"a", "b"}) == "ab");
  CHECK(total(std::list<int>{4, 5}) == 9);
}
