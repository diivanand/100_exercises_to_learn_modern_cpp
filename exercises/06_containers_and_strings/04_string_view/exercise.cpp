// =============================================================================
//  06.04 -- std::string_view
// =============================================================================
//
//  A `std::string_view` is a pointer and a length. It refers to characters it
//  does not own, and copying one copies sixteen bytes rather than a buffer.
//
//  It is the right parameter type for any function that READS a string and
//  does not keep it. One signature then accepts std::string, a literal, a
//  char buffer and another view, with no conversion or allocation at any call
//  site.
//
//  It also makes substring operations free: `remove_prefix`, `remove_suffix`
//  and `substr` move the pointer and adjust the length. `std::string::substr`
//  allocates.
//
//  THE DANGER, and it is a real one: a view does not keep its characters
//  alive. Every one of these is a dangling view:
//
//      std::string_view v = std::string{"temporary"};      // dies immediately
//      std::string_view f() { std::string s = ...; return s; }
//      view_member_ = some_temporary_string;
//
//  The rules that keep you safe:
//   * take a string_view as a PARAMETER, freely;
//   * do not STORE one unless you control the owner's lifetime;
//   * never return one that views a local.
//
//  One more trap: a string_view is NOT null-terminated. `view.data()` is not
//  a C string. Anything taking a `const char*` needs a real std::string.
//
//  TASK
//    Implement the four functions, all of which should allocate nothing.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 06_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Removes leading and trailing spaces and tabs. No allocation: the result
// views the same characters as the input.
//
// TODO: implement with remove_prefix / remove_suffix.
std::string_view trim(std::string_view text) {
  return text;
}

// Splits on `delimiter`. The pieces view into `text`, so the caller must keep
// `text` alive for as long as the result -- which is why this takes a
// string_view rather than a std::string it might be tempted to own.
//
// TODO: implement it. An empty input yields an empty vector; "a,,b" yields
// three pieces, the middle one empty.
std::vector<std::string_view> split(std::string_view text, char delimiter) {
  return {};
}

// True if `text` starts with `prefix`.
//
// TODO: C++20 added `starts_with` to both string and string_view, so this is
// one line.
bool has_prefix(std::string_view text, std::string_view prefix) {
  return false;
}

// Parses "key=value", returning the two halves as views. Returns two empty
// views if there is no '='.
//
// TODO: implement it. Note that the return type deliberately views into
// `line`: no copy is made, and the caller owns the characters.
struct KeyValue {
  std::string_view key;
  std::string_view value;
};

KeyValue parse_line(std::string_view line) {
  return {};
}

TEST_CASE("trim removes whitespace without copying") {
  const std::string owned = "   padded   ";
  const std::string_view trimmed = trim(owned);

  CHECK(trimmed == "padded");
  // The result must point into the original string, not at a copy.
  CHECK(trimmed.data() == owned.data() + 3);

  CHECK(trim("") == "");
  CHECK(trim("     ") == "");
  CHECK(trim("\t x \t") == "x");
  CHECK(trim("none") == "none");
}

TEST_CASE("split produces views, not strings") {
  const std::string csv = "a,bb,,ccc";
  const std::vector<std::string_view> parts = split(csv, ',');

  REQUIRE(parts.size() == 4);
  CHECK(parts[0] == "a");
  CHECK(parts[1] == "bb");
  CHECK(parts[2] == "");
  CHECK(parts[3] == "ccc");

  CHECK(parts[0].data() == csv.data());
  CHECK(split("", ',').empty());
  CHECK(split("single", ',').size() == 1);
}

TEST_CASE("has_prefix") {
  CHECK(has_prefix("modern C++", "modern"));
  CHECK(has_prefix("abc", "abc"));
  CHECK(has_prefix("abc", ""));
  CHECK_FALSE(has_prefix("abc", "abcd"));
  CHECK_FALSE(has_prefix("abc", "b"));
}

TEST_CASE("parse_line splits a key from a value") {
  const std::string line = "timeout=30";
  const KeyValue parsed = parse_line(line);

  CHECK(parsed.key == "timeout");
  CHECK(parsed.value == "30");
  CHECK(parsed.key.data() == line.data());

  const KeyValue empty_value = parse_line("flag=");
  CHECK(empty_value.key == "flag");
  CHECK(empty_value.value.empty());

  const KeyValue malformed = parse_line("nonsense");
  CHECK(malformed.key.empty());
  CHECK(malformed.value.empty());
}

TEST_CASE("a view is cheap to copy and does not own anything") {
  static_assert(sizeof(std::string_view) == 2 * sizeof(void*));
  static_assert(std::is_trivially_copyable_v<std::string_view>);
  CHECK(true);
}
