// Solution -- 12.04 std::regex
#include <doctest/doctest.h>

#include <optional>
#include <regex>
#include <string>
#include <vector>

bool is_identifier(const std::string& text) {
  // `static const`: constructing a std::regex compiles the pattern, which is
  // far more expensive than matching with it. Building one inside a function
  // that is called in a loop is the classic mistake with this header.
  static const std::regex pattern{R"([A-Za-z_][A-Za-z0-9_]*)"};
  return std::regex_match(text, pattern);
}

struct LogEntry {
  std::string date;
  std::string level;
  std::string message;
};

std::optional<LogEntry> parse_log(const std::string& line) {
  static const std::regex pattern{R"((\d{4}-\d{2}-\d{2}) (\w+) (.+))"};

  std::smatch match;
  if (!std::regex_match(line, match, pattern)) {
    return std::nullopt;
  }
  // [0] is the whole match; the groups start at 1.
  return LogEntry{match[1].str(), match[2].str(), match[3].str()};
}

std::vector<std::string> all_numbers(const std::string& text) {
  static const std::regex pattern{R"(\d+)"};

  std::vector<std::string> numbers;
  // A default-constructed sregex_iterator is the end sentinel.
  for (auto it = std::sregex_iterator{text.begin(), text.end(), pattern};
       it != std::sregex_iterator{}; ++it) {
    numbers.push_back(it->str());
  }
  return numbers;
}

std::string angle_brackets(const std::string& text) {
  static const std::regex pattern{R"(\{(\w+)\})"};
  return std::regex_replace(text, pattern, "<$1>");
}

TEST_CASE("regex_match requires the whole string to match") {
  CHECK(is_identifier("value"));
  CHECK(is_identifier("_private"));
  CHECK(is_identifier("count2"));
  CHECK(is_identifier("a"));

  CHECK_FALSE(is_identifier("2fast"));
  CHECK_FALSE(is_identifier("has space"));
  CHECK_FALSE(is_identifier(""));
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
