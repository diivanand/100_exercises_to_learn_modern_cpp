// Solution -- 13.02 Capstone: parsing
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

// std::from_chars: no allocation, no locale, no exceptions, and it tells you
// exactly where it stopped. The right tool for machine-readable input.
std::optional<double> parse_double(std::string_view text) {
  double value = 0.0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::int64_t> parse_int(std::string_view text) {
  std::int64_t value = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

// Parses  name{k=v,k=v} value timestamp_ms
Parsed<Reading> parse_line(std::string_view line) {
  const std::string_view trimmed = trim(line);
  if (trimmed.empty()) {
    return ParseError{ParseErrorKind::kEmptyLine, 0};
  }

  const std::size_t first_space = trimmed.find(' ');
  if (first_space == std::string_view::npos) {
    return ParseError{ParseErrorKind::kMissingValue, trimmed.size()};
  }

  std::string_view head = trimmed.substr(0, first_space);
  const std::string_view rest = trim(trimmed.substr(first_space + 1));

  const std::size_t second_space = rest.find(' ');
  if (second_space == std::string_view::npos) {
    return ParseError{ParseErrorKind::kMissingTimestamp, trimmed.size()};
  }

  const std::string_view value_text = rest.substr(0, second_space);
  const std::string_view time_text = trim(rest.substr(second_space + 1));

  const auto value = parse_double(value_text);
  if (!value) {
    return ParseError{ParseErrorKind::kBadNumber, first_space + 1};
  }

  const auto milliseconds = parse_int(time_text);
  if (!milliseconds) {
    return ParseError{ParseErrorKind::kBadNumber, first_space + 1 + second_space + 1};
  }

  Series series;
  if (const std::size_t brace = head.find('{'); brace != std::string_view::npos) {
    if (head.back() != '}') {
      return ParseError{ParseErrorKind::kBadTag, brace};
    }
    const std::string_view tag_text = head.substr(brace + 1, head.size() - brace - 2);
    head = head.substr(0, brace);

    for (const std::string_view entry : split(tag_text, ',')) {
      const std::size_t equals = entry.find('=');
      if (equals == std::string_view::npos || equals == 0) {
        return ParseError{ParseErrorKind::kBadTag,
                          static_cast<std::size_t>(entry.data() - trimmed.data())};
      }
      series.tags.emplace(entry.substr(0, equals), entry.substr(equals + 1));
    }
  }

  if (head.empty()) {
    return ParseError{ParseErrorKind::kEmptyName, 0};
  }
  series.name = head;

  return Reading{std::move(series),
                 Sample{Timestamp{std::chrono::milliseconds{*milliseconds}}, *value}};
}

// Parses a whole document, collecting the readings that parsed and the errors
// that did not -- with the line number, which is why the errors are values.
struct Document {
  std::vector<Reading> readings;
  std::vector<std::pair<std::size_t, ParseError>> errors;
};

Document parse_document(std::string_view text) {
  Document document;
  std::size_t line_number = 0;

  for (const std::string_view line : split(text, '\n')) {
    ++line_number;
    const std::string_view trimmed = trim(line);
    // Blank lines and comments are not errors.
    if (trimmed.empty() || trimmed.front() == '#') {
      continue;
    }
    auto result = parse_line(line);
    if (ok(result)) {
      document.readings.push_back(value_of(result));
    } else {
      document.errors.emplace_back(line_number, error_of(result));
    }
  }
  return document;
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
