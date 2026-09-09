// Solution -- 02.02 if constexpr
#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <type_traits>

template <typename T>
std::string to_string(const T& value) {
  // Each branch is only type-checked when it is the one selected, so
  // `std::to_string(value)` never has to be valid for std::string.
  if constexpr (std::is_same_v<T, bool>) {
    return value ? "true" : "false";
  } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    return std::to_string(value);
  } else {
    static_assert(std::is_convertible_v<T, std::string_view>,
                  "to_string: unsupported type");
    return std::string{std::string_view{value}};
  }
}

TEST_CASE("integers") {
  CHECK(to_string(42) == "42");
  CHECK(to_string(-7) == "-7");
  CHECK(to_string(7U) == "7");
}

TEST_CASE("bool is spelled out, not printed as 1 and 0") {
  CHECK(to_string(true) == "true");
  CHECK(to_string(false) == "false");
}

TEST_CASE("floating point") {
  CHECK(to_string(1.5) == std::to_string(1.5));
}

TEST_CASE("string-like things pass through") {
  CHECK(to_string(std::string{"already"}) == "already");
  CHECK(to_string(std::string_view{"a view"}) == "a view");
  CHECK(to_string("a literal") == "a literal");
}
