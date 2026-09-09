// =============================================================================
//  13.02 -- Capstone: parsing
// =============================================================================
//
//  Turning text into the types from 13.01. The input format is one reading per
//  line:
//
//      cpu.load 0.75 1000
//      http.requests{host=web-1,code=200} 42 2000
//      # comments and blank lines are ignored
//
//  What this exercise is really about:
//
//   * ERRORS AS VALUES (05.07). A malformed line is ordinary input, not an
//     exceptional event -- a log shipper meets thousands of them and must keep
//     going. So parse_line returns a value that is either a Reading or a
//     ParseError, and the caller cannot read the result without dealing with
//     the failure.
//
//   * NO ALLOCATION WHILE PARSING (06.04). Everything works on string_views
//     into the caller's buffer; a std::string is built only when a value is
//     actually stored.
//
//   * std::from_chars (C++17), which is the right way to parse a number from
//     machine-readable text: no allocation, no locale, no exceptions, and it
//     reports exactly where it stopped -- which is how "1.0x" becomes an error
//     rather than 1.0.
//
//  TASK
//    Write `parse_double`, `parse_int`, `parse_line` and `parse_document`.
//    `trim` and `split` are given -- they are 06.04's, unchanged.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 13_02
//
// =============================================================================

#include <doctest/doctest.h>

#include <charconv>
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

// ---------------------------------------------------------------------------
// The value types from 13.01, reduced to what parsing needs.
// ---------------------------------------------------------------------------

using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;

struct Sample {
  Timestamp at{};
  double value = 0.0;
  friend bool operator==(const Sample&, const Sample&) = default;
};

struct Series {
  std::string name;
  std::map<std::string, std::string> tags;
  friend bool operator==(const Series&, const Series&) = default;
};

struct Reading {
  Series series;
  Sample sample;
  friend bool operator==(const Reading&, const Reading&) = default;
};

// ---------------------------------------------------------------------------
// Errors as values (05.07): parsing user input is ordinary, and every failure
// carries where it happened.
// ---------------------------------------------------------------------------

enum class ParseErrorKind {
  kEmptyLine,
  kMissingValue,
  kMissingTimestamp,
  kBadNumber,
  kBadTag,
  kEmptyName,
};

struct ParseError {
  ParseErrorKind kind = ParseErrorKind::kEmptyLine;
  std::size_t column = 0;

  friend bool operator==(const ParseError&, const ParseError&) = default;
};

template <typename T>
using Parsed = std::variant<T, ParseError>;

template <typename T>
bool ok(const Parsed<T>& result) {
  return std::holds_alternative<T>(result);
}

template <typename T>
const T& value_of(const Parsed<T>& result) {
  return std::get<T>(result);
}

template <typename T>
ParseError error_of(const Parsed<T>& result) {
  return std::get<ParseError>(result);
}

// ---------------------------------------------------------------------------
// The parser. Every function takes a string_view and returns views into it
// where it can (06.04) -- no allocation until a value is actually stored.
// ---------------------------------------------------------------------------

std::string_view trim(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
    text.remove_suffix(1);
  }
  return text;
}

std::vector<std::string_view> split(std::string_view text, char delimiter) {
  std::vector<std::string_view> parts;
  if (text.empty()) {
    return parts;
  }
  std::size_t start = 0;
  while (true) {
    const std::size_t position = text.find(delimiter, start);
    if (position == std::string_view::npos) {
      parts.push_back(text.substr(start));
      return parts;
    }
    parts.push_back(text.substr(start, position - start));
    start = position + 1;
  }
}

// TODO: parse a number with std::from_chars, returning nothing on failure.
//
// It returns `{ptr, errc}` -- a structured binding (01.06) is how you read it.
// Two things must hold for the parse to have succeeded: `errc` is default
// constructed, AND the pointer reached the end of the input. Checking only the
// first accepts "1.0x" as 1.0, which is the bug the tests look for.
std::optional<double> parse_double(std::string_view text) {
  return std::nullopt;
}

// TODO: the same, for a 64-bit integer.
std::optional<std::int64_t> parse_int(std::string_view text) {
  return std::nullopt;
}

// TODO: parse one line into a Reading, or a ParseError saying what went wrong
// and roughly where.
//
// The grammar:   name[{key=value,...}] SP value SP timestamp_ms
//
// The failures the tests expect, in the order it is easiest to detect them:
//   kEmptyLine        nothing but whitespace
//   kMissingValue     no space at all -- just a name
//   kMissingTimestamp only two fields
//   kBadNumber        the value or the timestamp did not parse
//   kBadTag           a `{` with no closing `}`, or an entry with no `=`
//   kEmptyName        tags but no name in front of them
Parsed<Reading> parse_line(std::string_view line) {
  return ParseError{ParseErrorKind::kEmptyLine, 0};
}

// Parses a whole document, collecting the readings that parsed and the errors
// that did not -- with the line number, which is why the errors are values.
struct Document {
  std::vector<Reading> readings;
  std::vector<std::pair<std::size_t, ParseError>> errors;
};

// TODO: parse every line, keeping the readings that worked and the errors that
// did not -- with 1-BASED line numbers that count the blank and comment lines
// you skipped.
//
// Blank lines and lines starting with '#' are not errors.
Document parse_document(std::string_view text) {
  return {};
}

TEST_CASE("a bare metric") {
  const auto result = parse_line("cpu.load 0.75 1000");
  REQUIRE(ok(result));

  const Reading& reading = value_of(result);
  CHECK(reading.series.name == "cpu.load");
  CHECK(reading.series.tags.empty());
  CHECK(reading.sample.value == doctest::Approx(0.75));
  CHECK(reading.sample.at == Timestamp{std::chrono::milliseconds{1000}});
}

TEST_CASE("tags") {
  const auto result = parse_line("http.requests{host=web-1,code=200} 42 2000");
  REQUIRE(ok(result));

  const Reading& reading = value_of(result);
  CHECK(reading.series.name == "http.requests");
  CHECK(reading.series.tags.size() == 2);
  CHECK(reading.series.tags.at("host") == "web-1");
  CHECK(reading.series.tags.at("code") == "200");
  CHECK(reading.sample.value == doctest::Approx(42.0));
}

TEST_CASE("whitespace is tolerated") {
  const auto result = parse_line("   cpu.load   0.5   500   ");
  REQUIRE(ok(result));
  CHECK(value_of(result).series.name == "cpu.load");
  CHECK(value_of(result).sample.value == doctest::Approx(0.5));
}

TEST_CASE("negative and scientific values") {
  CHECK(value_of(parse_line("temp -3.5 1")).sample.value == doctest::Approx(-3.5));
  CHECK(value_of(parse_line("tiny 1.5e-3 1")).sample.value == doctest::Approx(0.0015));
}

TEST_CASE("every failure says what and where") {
  CHECK(error_of(parse_line("")).kind == ParseErrorKind::kEmptyLine);
  CHECK(error_of(parse_line("   ")).kind == ParseErrorKind::kEmptyLine);
  CHECK(error_of(parse_line("lonely")).kind == ParseErrorKind::kMissingValue);
  CHECK(error_of(parse_line("name 1.0")).kind == ParseErrorKind::kMissingTimestamp);
  CHECK(error_of(parse_line("name abc 100")).kind == ParseErrorKind::kBadNumber);
  CHECK(error_of(parse_line("name 1.0 abc")).kind == ParseErrorKind::kBadNumber);
  CHECK(error_of(parse_line("name{bad} 1.0 100")).kind == ParseErrorKind::kBadTag);
  CHECK(error_of(parse_line("{host=a} 1.0 100")).kind == ParseErrorKind::kEmptyName);
}

TEST_CASE("a trailing character makes a number invalid") {
  // from_chars stops at the first character it cannot use, and checking that
  // it consumed everything is what turns "1.0x" into an error rather than 1.0.
  CHECK(error_of(parse_line("name 1.0x 100")).kind == ParseErrorKind::kBadNumber);
  CHECK(error_of(parse_line("name 1.0 100ms")).kind == ParseErrorKind::kBadNumber);
}

TEST_CASE("a document collects results and errors together") {
  const auto document = parse_document("# a comment\n"
                                       "cpu.load 0.5 100\n"
                                       "\n"
                                       "broken line here\n"
                                       "mem.used{host=a} 1024 200\n"
                                       "also bad\n");

  REQUIRE(document.readings.size() == 2);
  CHECK(document.readings[0].series.name == "cpu.load");
  CHECK(document.readings[1].series.tags.at("host") == "a");

  REQUIRE(document.errors.size() == 2);
  // The line numbers are 1-based and count blank lines and comments.
  CHECK(document.errors[0].first == 4);
  CHECK(document.errors[1].first == 6);
}

TEST_CASE("an empty document") {
  const auto document = parse_document("");
  CHECK(document.readings.empty());
  CHECK(document.errors.empty());

  const auto comments = parse_document("# only\n# comments\n");
  CHECK(comments.readings.empty());
  CHECK(comments.errors.empty());
}
