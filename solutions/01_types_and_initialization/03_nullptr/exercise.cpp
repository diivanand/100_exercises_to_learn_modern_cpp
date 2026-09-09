// Solution -- 01.03 nullptr
#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <string>

std::string describe(int) {
  return "int";
}
std::string describe(const char*) {
  return "pointer";
}
std::string describe(std::nullptr_t) {
  return "nullptr_t";
}

std::string call_with_null() {
  // A null pointer that has a pointer *type*, so the const char* overload wins.
  const char* text = nullptr;
  return describe(text);
}

std::optional<char> first_char(const char* text) {
  if (text == nullptr || text[0] == '\0') {
    return std::nullopt;
  }
  return text[0];
}

TEST_CASE("overload resolution distinguishes 0, nullptr and a null pointer") {
  CHECK(describe(0) == "int");
  CHECK(describe(nullptr) == "nullptr_t");
  CHECK(call_with_null() == "pointer");
}

TEST_CASE("first_char guards against null") {
  CHECK(first_char("abc") == 'a');
  CHECK_FALSE(first_char("").has_value());
  CHECK_FALSE(first_char(nullptr).has_value());
}
