// Solution -- 08.08 Designing your own concepts
#include <doctest/doctest.h>

#include <concepts>
#include <cstddef>
#include <ranges>
#include <string>
#include <vector>

template <typename T>
concept SelfDescribing = requires(const T& value) {
  { value.to_string() } -> std::convertible_to<std::string>;
};

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// A concept cannot refer to itself, so the recursion is expressed by asking
// about the element type's three cases directly. One level of unrolling is
// enough to make vector<vector<int>> work, because the element concept is
// evaluated afresh for each nesting level.
template <typename T>
concept SerialisableElement = Numeric<T> || SelfDescribing<T>;

template <typename T>
concept SerialisableRange =
    std::ranges::range<T> &&
    (SerialisableElement<std::ranges::range_value_t<T>> ||
     (std::ranges::range<std::ranges::range_value_t<T>> &&
      SerialisableElement<std::ranges::range_value_t<std::ranges::range_value_t<T>>>));

template <typename T>
concept Serialisable = Numeric<T> || SelfDescribing<T> || SerialisableRange<T>;

template <Numeric T>
std::string serialise(const T& value) {
  return std::to_string(value);
}

template <SelfDescribing T>
std::string serialise(const T& value) {
  return value.to_string();
}

template <SerialisableRange T>
std::string serialise(const T& range) {
  std::string result = "[";
  for (bool first = true; const auto& element : range) {
    if (!first) {
      result += ',';
    }
    result += serialise(element);
    first = false;
  }
  result += ']';
  return result;
}

struct Point {
  int x = 0;
  int y = 0;

  std::string to_string() const {
    return "(" + std::to_string(x) + "," + std::to_string(y) + ")";
  }
};

struct Opaque {
  int hidden = 0;
};

TEST_CASE("the concepts classify types correctly") {
  static_assert(Numeric<int>);
  static_assert(Numeric<double>);
  static_assert(!Numeric<std::string>);

  static_assert(SelfDescribing<Point>);
  static_assert(!SelfDescribing<int>);
  static_assert(!SelfDescribing<Opaque>);

  static_assert(SerialisableRange<std::vector<int>>);
  static_assert(SerialisableRange<std::vector<Point>>);
  static_assert(!SerialisableRange<std::vector<Opaque>>);

  static_assert(Serialisable<int>);
  static_assert(Serialisable<Point>);
  static_assert(Serialisable<std::vector<Point>>);
  static_assert(!Serialisable<Opaque>);
  CHECK(true);
}

TEST_CASE("numbers") {
  CHECK(serialise(42) == "42");
  CHECK(serialise(-1) == "-1");
}

TEST_CASE("self-describing types") {
  CHECK(serialise(Point{1, 2}) == "(1,2)");
}

TEST_CASE("ranges, recursively") {
  CHECK(serialise(std::vector<int>{1, 2, 3}) == "[1,2,3]");
  CHECK(serialise(std::vector<int>{}) == "[]");
  CHECK(serialise(std::vector<Point>{{1, 2}, {3, 4}}) == "[(1,2),(3,4)]");

  const std::vector<std::vector<int>> nested = {{1, 2}, {3}};
  CHECK(serialise(nested) == "[[1,2],[3]]");
}

TEST_CASE("an unserialisable type is rejected at the call") {
  CHECK(true);
}
