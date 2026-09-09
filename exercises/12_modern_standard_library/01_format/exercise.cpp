// =============================================================================
//  12.01 -- std::format
// =============================================================================
//
//  C++20 finally has string formatting that is safe, fast and readable.
//
//      std::format("{} is {} years old", name, age)
//      std::format("{:>10}", text)      right-aligned in 10 columns
//      std::format("{:.3f}", value)     three decimal places
//      std::format("{:#x}", value)      0x-prefixed hex
//      std::format("{1} {0}", a, b)     by index
//      std::format("{:{}}", text, w)    width taken from an argument
//
//  What it fixes:
//
//   * printf is not type-safe. `printf("%d", "text")` is undefined behaviour;
//     std::format's string is checked AT COMPILE TIME, and a mismatched
//     argument is a compile error.
//   * iostreams are type-safe but verbose, and formatting state
//     (`std::setw`, `std::setprecision`) is sticky and global.
//   * Neither can be localised or reordered by a translator; `{1} {0}` can.
//
//  It is also faster than both, because it writes into the output buffer
//  directly rather than building intermediate strings.
//
//  MAKING YOUR OWN TYPES FORMATTABLE: specialise `std::formatter<T>` with
//  `parse` and `format`. The usual shortcut is to inherit the parsing from an
//  existing formatter -- most often `std::formatter<std::string>` -- and only
//  write `format`.
//
//  TASK
//    Replace the manual string building, then make `Point` formattable.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 12_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <format>
#include <string>
#include <vector>

// TODO: rewrite with std::format.
std::string describe(const std::string& name, int age) {
  return name + " is " + std::to_string(age) + " years old";
}

// Formats a table row: the name left-aligned in 12 columns, the amount right
// aligned in 8 with two decimal places.
//
// TODO: one std::format call. The spec you need is "{:<12}{:>8.2f}".
std::string table_row(const std::string& name, double amount) {
  return name + "  " + std::to_string(amount);
}

// TODO: format `value` as hexadecimal with a 0x prefix, padded with zeroes to
// `width` digits. The width comes from an argument, which is what the nested
// "{}" in a spec is for.
std::string as_hex(unsigned value, int width) {
  return {};
}

struct Point {
  int x = 0;
  int y = 0;
};

// TODO: specialise std::formatter<Point> so that `std::format("{}", point)`
// produces "(1, 2)".
//
// The short version inherits parsing from the string formatter:
//
//   template <>
//   struct std::formatter<Point> : std::formatter<std::string> {
//     auto format(const Point& p, std::format_context& ctx) const {
//       return std::formatter<std::string>::format(
//           std::format("({}, {})", p.x, p.y), ctx);
//     }
//   };

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

  // Inheriting from std::formatter<std::string> means the standard alignment
  // specs work on a Point too.
  CHECK(std::format("{:>10}", Point{1, 2}) == "    (1, 2)");
}

TEST_CASE("the format string is checked at compile time") {
  // Each of these is a compile error, not a runtime surprise. Uncomment one:
  //
  //   std::format("{} {}", 1);        // too few arguments
  //   std::format("{:d}", "text");    // a string is not an integer
  //   std::format("{", 1);            // unmatched brace
  //
  // std::vformat is the runtime-checked version, for when the format string
  // genuinely comes from data.
  // std::make_format_args binds to lvalues, so the arguments need names.
  int first = 1;
  int second = 2;
  CHECK(std::vformat("{} {}", std::make_format_args(first, second)) == "1 2");
}
