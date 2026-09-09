// Solution -- 08.06 From SFINAE to concepts
#include <doctest/doctest.h>

#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

template <std::integral T>
std::string describe(T) {
  return "integer";
}

template <std::floating_point T>
std::string describe(T) {
  return "floating point";
}

template <std::convertible_to<std::string_view> T>
std::string describe(T) {
  return "string-like";
}

template <typename T>
concept Sized = requires(const T& value) {
  { value.size() } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept Indexable = requires(const T& value, std::size_t index) { value[index]; };

// A named composition, so the requirement can be reused and reads as one idea.
template <typename T>
concept SizedSequence = Sized<T> && Indexable<T>;

template <Sized T>
std::size_t size_of(const T& value) {
  return value.size();
}

template <SizedSequence T>
auto first_element(const T& container) {
  return container[0];
}

TEST_CASE("overload resolution picks by concept") {
  CHECK(describe(42) == "integer");
  CHECK(describe(42L) == "integer");
  CHECK(describe('c') == "integer");
  CHECK(describe(1.5) == "floating point");
  CHECK(describe(1.5F) == "floating point");
  CHECK(describe("literal") == "string-like");
  CHECK(describe(std::string{"owned"}) == "string-like");
}

TEST_CASE("a concept about a member, not a type trait") {
  CHECK(size_of(std::string{"abc"}) == 3);
  CHECK(size_of(std::vector<int>{1, 2}) == 2);
}

TEST_CASE("composed concepts") {
  CHECK(first_element(std::vector<int>{7, 8}) == 7);
  CHECK(first_element(std::string{"xy"}) == 'x');
}

TEST_CASE("concepts are ordinary compile-time predicates") {
  static_assert(std::integral<int>);
  static_assert(!std::integral<double>);
  static_assert(std::floating_point<double>);
  static_assert(std::convertible_to<const char*, std::string_view>);

  static_assert(Sized<std::string>);
  static_assert(!Sized<int>);
  static_assert(SizedSequence<std::vector<int>>);
  CHECK(true);
}
