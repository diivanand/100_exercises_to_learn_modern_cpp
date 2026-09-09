// =============================================================================
//  01.07 -- if and switch with an initialiser
// =============================================================================
//
//  C++17 let `if` and `switch` take an init-statement, the way `for` always
//  could:
//
//      if (auto it = m.find(key); it != m.end()) { use(it); }
//      //  ^^^^^^^^^^^^^^^^^^^^^  scoped to the if/else, and nowhere else
//
//  Two reasons this matters, and neither is saving a line.
//
//   1. SCOPE. The variable dies at the closing brace, so it cannot be reused
//      by accident three statements later, and the name is free again.
//      Core Guidelines ES.5: "Keep scopes small".
//
//   2. IT READS AS ONE THOUGHT. "Look it up, and if you found it..." is one
//      idea; splitting it across two statements pretends it is two.
//
//  `switch` takes the same form: `switch (auto state = poll(); state.kind)`.
//
//  TASK
//    Rewrite `lookup_or` and `classify` to use init-statements, and fix the
//    bugs that the wider scope was hiding.
//
//  RUN IT
//    ./mcpp test 01_07
//
// =============================================================================

#include <doctest/doctest.h>

#include <map>
#include <string>
#include <string_view>

// Returns the value stored under `key`, or `fallback` if there is none.
//
// TODO: rewrite as
//     if (const auto it = table.find(key); it != table.end()) { ... }
// The version below leaks `it` into the rest of the function -- and then uses
// it after the check has already failed.
int lookup_or(const std::map<std::string, int>& table, const std::string& key,
              int fallback) {
  auto it = table.find(key);
  if (it == table.end()) {
    // Someone "helpfully" added a second lookup here later, reusing `it`.
    it = table.begin();
  }
  return it == table.end() ? fallback : it->second;
}

enum class Size { kEmpty, kSmall, kLarge };

// Classifies a string by length: 0 is empty, up to 3 is small, otherwise large.
//
// TODO: rewrite as `switch (const Size size = size_of(text); size)` and give
// every enumerator its own case. Note that the `default` label below is what
// lets the missing case slip through silently -- without it, this project's
// -Werror build would reject the switch for not handling `kLarge`.
Size size_of(std::string_view text) {
  if (text.empty()) {
    return Size::kEmpty;
  }
  return text.size() <= 3 ? Size::kSmall : Size::kLarge;
}

std::string classify(std::string_view text) {
  const Size size = size_of(text);
  switch (size) {
  case Size::kEmpty:
    return "empty";
  default:
    return "small";
  }
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
