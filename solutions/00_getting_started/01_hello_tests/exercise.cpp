// Solution -- 00.01 Hello, tests
#include <doctest/doctest.h>

#include <string>
#include <string_view>

std::string greet(std::string_view name) {
  // Reserve once so the appends below do not reallocate. This is a
  // micro-optimisation and not the point of the exercise; the point is that
  // building a std::string from parts starts with a std::string.
  std::string result;
  result.reserve(name.size() + 8);
  result += "Hello, ";
  result += name;
  result += '!';
  return result;
}

TEST_CASE("greet builds a greeting") {
  CHECK(greet("world") == "Hello, world!");
  CHECK(greet("modern C++") == "Hello, modern C++!");
}

TEST_CASE("greet accepts anything string-like") {
  const std::string owned = "Diivanand";
  CHECK(greet(owned) == "Hello, Diivanand!");

  constexpr std::string_view view = "view";
  CHECK(greet(view) == "Hello, view!");
}
