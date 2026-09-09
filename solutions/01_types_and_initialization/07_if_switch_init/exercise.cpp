// Solution -- 01.07 if and switch with an initialiser
#include <doctest/doctest.h>

#include <map>
#include <string>
#include <string_view>

int lookup_or(const std::map<std::string, int>& table, const std::string& key,
              int fallback) {
  if (const auto it = table.find(key); it != table.end()) {
    return it->second;
  }
  return fallback;
}

enum class Size { kEmpty, kSmall, kLarge };

Size size_of(std::string_view text) {
  if (text.empty()) {
    return Size::kEmpty;
  }
  return text.size() <= 3 ? Size::kSmall : Size::kLarge;
}

std::string classify(std::string_view text) {
  // No `default` label: every enumerator is handled explicitly, so adding a
  // fourth Size later makes the compiler point at this switch instead of
  // letting it quietly fall into a catch-all.
  switch (const Size size = size_of(text); size) {
  case Size::kEmpty:
    return "empty";
  case Size::kSmall:
    return "small";
  case Size::kLarge:
    return "large";
  }
  return "unreachable";
}

TEST_CASE("lookup_or falls back when the key is missing") {
  const std::map<std::string, int> table = {{"a", 1}, {"b", 2}};
  CHECK(lookup_or(table, "a", -1) == 1);
  CHECK(lookup_or(table, "b", -1) == 2);
  CHECK(lookup_or(table, "zzz", -1) == -1);

  const std::map<std::string, int> empty;
  CHECK(lookup_or(empty, "a", 99) == 99);
}

TEST_CASE("classify covers every size") {
  CHECK(classify("") == "empty");
  CHECK(classify("ab") == "small");
  CHECK(classify("abc") == "small");
  CHECK(classify("abcd") == "large");
}
