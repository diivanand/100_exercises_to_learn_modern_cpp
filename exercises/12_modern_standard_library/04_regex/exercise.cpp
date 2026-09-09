// =============================================================================
//  12.04 -- std::regex
// =============================================================================
//
//  `<regex>` gives you regular expressions in the standard library. It is
//  worth knowing, and worth knowing when NOT to use.
//
//  The three algorithms:
//
//      regex_match    the WHOLE string must match
//      regex_search   find a match anywhere
//      regex_replace  substitute, with $1, $2 for capture groups
//
//  and `sregex_iterator` to walk every match.
//
//  `std::smatch` holds the results: `[0]` is the whole match, `[1]` onward are
//  the capture groups, and each has `.str()`, `.position()` and `.length()`.
//
//  Two practical points:
//
//   * BUILD THE REGEX ONCE. Constructing a std::regex compiles it, which is
//     expensive. A regex built inside a loop is the most common performance
//     bug in code that uses this header. Make it `static const`, or a member.
//
//   * std::regex IS SLOW -- famously so, an order of magnitude behind PCRE or
//     RE2. For a hot path, or for untrusted patterns (where catastrophic
//     backtracking is a denial-of-service risk), use a real regex library or
//     write the parser by hand. For configuration files and log parsing it is
//     entirely adequate.
//
//  Escaping is doubled in C++ source: `\\d` in a normal string literal, or
//  `\d` in a RAW STRING LITERAL, `R"(\d+)"`. Prefer raw strings.
//
//  TASK
//    Implement the four functions.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 12_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <optional>
#include <regex>
#include <string>
#include <vector>

// TODO: true if `text` is a valid identifier: a letter or underscore, then any
// number of letters, digits or underscores -- and nothing else.
//
// Use regex_match (the whole string), a raw string literal for the pattern,
// and build the regex once.
bool is_identifier(const std::string& text) {
  return false;
}

// A log line looks like:  2024-03-15 ERROR Disk full
//
// TODO: extract the three parts. Return an empty optional if the line does not
// match. Capture groups are what `smatch[1]`, `[2]`, `[3]` refer to.
struct LogEntry {
  std::string date;
  std::string level;
  std::string message;
};

std::optional<LogEntry> parse_log(const std::string& line) {
  return std::nullopt;
}

// TODO: return every number in the text, in order. sregex_iterator walks the
// matches; its default-constructed form is the end sentinel.
std::vector<std::string> all_numbers(const std::string& text) {
  return {};
}

// TODO: replace every occurrence of "{name}" with the name in angle brackets:
// "hello {world}" becomes "hello <world>". regex_replace, and `$1` in the
// replacement refers to the first capture group.
std::string angle_brackets(const std::string& text) {
  return {};
}

TEST_CASE("regex_match requires the whole string to match") {
  CHECK(is_identifier("value"));
  CHECK(is_identifier("_private"));
  CHECK(is_identifier("count2"));
  CHECK(is_identifier("a"));

  CHECK_FALSE(is_identifier("2fast"));
  CHECK_FALSE(is_identifier("has space"));
  CHECK_FALSE(is_identifier(""));
  // regex_search would accept this, because "ok" matches somewhere inside it.
  CHECK_FALSE(is_identifier("ok-not"));
}

TEST_CASE("capture groups") {
  const auto entry = parse_log("2024-03-15 ERROR Disk full");
  REQUIRE(entry.has_value());
  CHECK(entry->date == "2024-03-15");
  CHECK(entry->level == "ERROR");
  CHECK(entry->message == "Disk full");

  const auto with_spaces = parse_log("2024-01-01 INFO started up cleanly");
  REQUIRE(with_spaces.has_value());
  CHECK(with_spaces->message == "started up cleanly");

  CHECK_FALSE(parse_log("not a log line").has_value());
  CHECK_FALSE(parse_log("").has_value());
}

TEST_CASE("iterating every match") {
  CHECK(all_numbers("a1b22c333") == std::vector<std::string>{"1", "22", "333"});
  CHECK(all_numbers("no digits here").empty());
  CHECK(all_numbers("42") == std::vector<std::string>{"42"});
}

TEST_CASE("replacing with a capture group") {
  CHECK(angle_brackets("hello {world}") == "hello <world>");
  CHECK(angle_brackets("{a} and {b}") == "<a> and <b>");
  CHECK(angle_brackets("nothing to do") == "nothing to do");
}
