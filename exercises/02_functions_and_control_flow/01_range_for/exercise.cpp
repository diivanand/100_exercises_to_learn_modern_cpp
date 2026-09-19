// =============================================================================
//  02.01 -- Range-based for, and the loop you should not have written
// =============================================================================
//
//  The indexed loop is where off-by-one errors, signed/unsigned mismatches and
//  iterator-invalidation bugs come from. The range-based for loop removes the
//  index, and with it the whole category:
//
//      for (const auto& item : container) { ... }
//
//  Core Guidelines ES.71: "Prefer a range-for-statement to a for-statement
//  when there is a choice". There is almost always a choice.
//
//  Pick the loop variable the same way you pick a parameter type:
//
//      for (const auto& x : c)   read-only, no copy      <- the default
//      for (auto& x : c)         you intend to modify
//      for (auto x : c)          cheap scalar you want a copy of
//
//  C++20 also gave range-for an init-statement, which is how you keep a
//  loop-scoped helper out of the enclosing scope:
//
//      for (std::size_t index = 0; const auto& x : c) { ...; ++index; }
//
//  TASK
//    Rewrite all three loops. Each currently has a bug that indexing made easy
//    to write and hard to see.
//
//  RUN IT
//    ./mcpp test 02_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

// TODO: rewrite as a range-for. The `<=` is the classic off-by-one, and it
// reads past the end of the vector -- undefined behaviour that happens to
// look like it works.
int sum(const std::vector<int>& values) {
  int total = 0;
  for (const auto& value : values) {
    total += value;
  }
  return total;
}

// Joins the values with `separator` between them (not after the last).
//
// TODO: rewrite using a range-for with an init-statement:
//     for (bool first = true; const auto& value : values) { ... }
// The index version below appends a trailing separator.
std::string join(const std::vector<std::string>& values, const std::string& separator) {
  std::string result;
  for (bool first = true; const auto& value : values) {
    if (!first) {
      result += separator;
    }
    result += value;
    first = false;
  }
  return result;
}

// Rounds every value to the nearest multiple of `step`.
//
// TODO: rewrite as a range-for over references. The loop below writes to a
// copy of the element, because `values[i]` is read into `value` first.
void quantise(std::vector<int>& values, int step) {
  for (auto& value : values) {
    value = (value / step) * step;
  }
}

TEST_CASE("sum adds every element exactly once") {
  CHECK(sum({1, 2, 3, 4}) == 10);
  CHECK(sum({}) == 0);
  CHECK(sum({-5, 5}) == 0);
}

TEST_CASE("join puts the separator between values") {
  CHECK(join({"a", "b", "c"}, ", ") == "a, b, c");
  CHECK(join({"only"}, ", ") == "only");
  CHECK(join({}, ", ").empty());
}

TEST_CASE("quantise rounds down to a multiple of step") {
  std::vector<int> values = {0, 7, 13, 25};
  quantise(values, 5);
  CHECK(values == std::vector<int>{0, 5, 10, 25});
}
