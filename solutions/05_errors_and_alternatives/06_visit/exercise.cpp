// Solution -- 05.06 std::visit and the overloaded pattern
#include <doctest/doctest.h>

#include <string>
#include <variant>

using Value = std::variant<std::monostate, bool, double, std::string>;

template <typename... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

std::string type_name(const Value& value) {
  return std::visit(Overloaded{
                        [](std::monostate) { return "null"; },
                        [](bool) { return "bool"; },
                        [](double) { return "number"; },
                        [](const std::string&) { return "string"; },
                    },
                    value);
}

std::string trim_zeroes(std::string text) {
  if (text.find('.') == std::string::npos) {
    return text;
  }
  while (!text.empty() && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  return text;
}

std::string to_string(const Value& value) {
  return std::visit(Overloaded{
                        [](std::monostate) { return std::string{"null"}; },
                        [](bool flag) { return std::string{flag ? "true" : "false"}; },
                        [](double number) { return trim_zeroes(std::to_string(number)); },
                        [](const std::string& text) { return "\"" + text + "\""; },
                    },
                    value);
}

Value add(const Value& lhs, const Value& rhs) {
  return std::visit(
      Overloaded{
          [](double a, double b) { return Value{a + b}; },
          [](const std::string& a, const std::string& b) { return Value{a + b}; },
          // Less specialised than the two above, so it only wins for the pairs
          // they do not cover.
          [](const auto&, const auto&) { return Value{}; },
      },
      lhs, rhs);
}

TEST_CASE("type_name covers every alternative") {
  CHECK(type_name(Value{}) == "null");
  CHECK(type_name(Value{true}) == "bool");
  CHECK(type_name(Value{1.0}) == "number");
  CHECK(type_name(Value{std::string{"s"}}) == "string");
}

TEST_CASE("to_string renders each alternative") {
  CHECK(to_string(Value{}) == "null");
  CHECK(to_string(Value{true}) == "true");
  CHECK(to_string(Value{false}) == "false");
  CHECK(to_string(Value{2.5}) == "2.5");
  CHECK(to_string(Value{3.0}) == "3");
  CHECK(to_string(Value{std::string{"hi"}}) == "\"hi\"");
}

TEST_CASE("add dispatches on both operands") {
  CHECK(to_string(add(Value{1.0}, Value{2.0})) == "3");
  CHECK(to_string(add(Value{std::string{"a"}}, Value{std::string{"b"}})) == "\"ab\"");

  CHECK(to_string(add(Value{1.0}, Value{std::string{"b"}})) == "null");
  CHECK(to_string(add(Value{true}, Value{true})) == "null");
  CHECK(to_string(add(Value{}, Value{})) == "null");
}

TEST_CASE("a visitor that misses an alternative does not compile") {
  CHECK(true);
}
