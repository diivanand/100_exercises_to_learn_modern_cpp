// Solution -- 05.05 std::variant
#include <doctest/doctest.h>

#include <string>
#include <variant>
#include <vector>

using Value = std::variant<std::monostate, bool, double, std::string>;

std::string type_name(const Value& value) {
  if (std::holds_alternative<std::monostate>(value)) {
    return "null";
  }
  if (std::holds_alternative<bool>(value)) {
    return "bool";
  }
  if (std::holds_alternative<double>(value)) {
    return "number";
  }
  return "string";
}

double as_number(const Value& value) {
  // get_if returns a pointer to the alternative, or nullptr. No exception, no
  // prior holds_alternative check that could drift out of sync with the get.
  if (const double* number = std::get_if<double>(&value)) {
    return *number;
  }
  return 0.0;
}

bool is_falsy(const Value& value) {
  if (std::holds_alternative<std::monostate>(value)) {
    return true;
  }
  if (const bool* flag = std::get_if<bool>(&value)) {
    return !*flag;
  }
  if (const double* number = std::get_if<double>(&value)) {
    return *number == 0.0;
  }
  return std::get<std::string>(value).empty();
}

TEST_CASE("a default-constructed Value is null") {
  const Value value;
  CHECK(type_name(value) == "null");
  CHECK(value.index() == 0);
  CHECK(std::holds_alternative<std::monostate>(value));
}

TEST_CASE("each alternative is recognised") {
  CHECK(type_name(Value{true}) == "bool");
  CHECK(type_name(Value{3.5}) == "number");
  CHECK(type_name(Value{std::string{"text"}}) == "string");
}

TEST_CASE("assignment changes the alternative") {
  Value value = 1.5;
  CHECK(type_name(value) == "number");

  value = std::string{"now a string"};
  CHECK(type_name(value) == "string");
  CHECK(std::get<std::string>(value) == "now a string");

  CHECK_THROWS_AS((void)std::get<double>(value), std::bad_variant_access);
}

TEST_CASE("as_number does not throw for other types") {
  CHECK(as_number(Value{42.0}) == doctest::Approx(42.0));
  CHECK(as_number(Value{std::string{"x"}}) == doctest::Approx(0.0));
  CHECK(as_number(Value{}) == doctest::Approx(0.0));
}

TEST_CASE("falsiness") {
  CHECK(is_falsy(Value{}));
  CHECK(is_falsy(Value{false}));
  CHECK(is_falsy(Value{0.0}));
  CHECK(is_falsy(Value{std::string{}}));

  CHECK_FALSE(is_falsy(Value{true}));
  CHECK_FALSE(is_falsy(Value{0.5}));
  CHECK_FALSE(is_falsy(Value{std::string{"x"}}));
}

TEST_CASE("a variant is as big as its largest alternative, plus a tag") {
  static_assert(sizeof(Value) >= sizeof(std::string));
  CHECK(true);
}
