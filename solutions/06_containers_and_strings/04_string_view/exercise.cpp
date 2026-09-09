// Solution -- 06.04 std::string_view
#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

bool is_space(char c) {
  return c == ' ' || c == '\t';
}

} // namespace

std::string_view trim(std::string_view text) {
  while (!text.empty() && is_space(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && is_space(text.back())) {
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

bool has_prefix(std::string_view text, std::string_view prefix) {
  return text.starts_with(prefix);
}

struct KeyValue {
  std::string_view key;
  std::string_view value;
};

KeyValue parse_line(std::string_view line) {
  const std::size_t position = line.find('=');
  if (position == std::string_view::npos) {
    return {};
  }
  // substr on a view is pointer arithmetic, not an allocation.
  return {line.substr(0, position), line.substr(position + 1)};
}

TEST_CASE("trim removes whitespace without copying") {
  const std::string owned = "   padded   ";
  const std::string_view trimmed = trim(owned);

  CHECK(trimmed == "padded");
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
