// Solution -- 12.01 std::format
#include <doctest/doctest.h>

#include <format>
#include <string>

std::string describe(const std::string& name, int age) {
  return std::format("{} is {} years old", name, age);
}

std::string table_row(const std::string& name, double amount) {
  return std::format("{:<12}{:>8.2f}", name, amount);
}

std::string as_hex(unsigned value, int width) {
  // The nested "{}" takes the width from the next argument, so it does not
  // have to be known when the format string is written.
  return std::format("0x{:0{}x}", value, width);
}

struct Point {
  int x = 0;
  int y = 0;
};

template <>
struct std::formatter<Point> : std::formatter<std::string> {
  // parse() is inherited from the string formatter, so every alignment, width
  // and fill spec works on a Point without any extra code.
  auto format(const Point& point, std::format_context& context) const {
    return std::formatter<std::string>::format(std::format("({}, {})", point.x, point.y),
                                               context);
  }
};

TEST_CASE("basic substitution") {
  CHECK(describe("ada", 36) == "ada is 36 years old");
  CHECK(describe("", 0) == " is 0 years old");
}

TEST_CASE("alignment and precision") {
  CHECK(table_row("widget", 12.5) == "widget         12.50");
  CHECK(table_row("a", 0.0) == "a               0.00");
  CHECK(table_row("a very long name", 1.0) == "a very long name    1.00");
}

TEST_CASE("a width taken from an argument") {
  CHECK(as_hex(255, 4) == "0x00ff");
  CHECK(as_hex(255, 2) == "0xff");
  CHECK(as_hex(0, 4) == "0x0000");
}

TEST_CASE("a user-defined type") {
  CHECK(std::format("{}", Point{1, 2}) == "(1, 2)");
  CHECK(std::format("from {} to {}", Point{0, 0}, Point{3, 4}) ==
        "from (0, 0) to (3, 4)");
  CHECK(std::format("{:>10}", Point{1, 2}) == "    (1, 2)");
}

TEST_CASE("the format string is checked at compile time") {
  // std::make_format_args binds to lvalues, so the arguments need names.
  int first = 1;
  int second = 2;
  CHECK(std::vformat("{} {}", std::make_format_args(first, second)) == "1 2");
}
