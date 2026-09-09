// =============================================================================
//  05.04 -- std::optional
// =============================================================================
//
//  `std::optional<T>` holds a T or nothing, in the object itself -- no
//  allocation, no null pointer, no sentinel value that a caller might forget
//  to check.
//
//  It replaces three bad habits:
//
//      int find(...);                  // returns -1 for "not found"
//      bool find(..., int& out);       // out-parameter and a bool
//      Widget* find(...);              // nullptr means... what, exactly?
//
//  What it is NOT: an error type. `std::optional` says "there is no value" and
//  nothing about why. When the caller needs the reason, you want an
//  error-carrying type instead (05.07).
//
//  The interface, in the order you will reach for it:
//
//      opt.has_value(), or just `if (opt)`
//      *opt / opt->field         fast, undefined if empty -- check first
//      opt.value()               throws std::bad_optional_access if empty
//      opt.value_or(fallback)    the one you usually want
//      opt.reset(), opt = std::nullopt
//      opt.emplace(args...)      construct in place
//
//  Note that `if (opt)` and `if (*opt)` mean different things for
//  `optional<bool>`. That is a real trap; prefer `has_value()` there.
//
//  TASK
//    Replace the sentinel-and-out-parameter interfaces below with
//    std::optional, and implement `first_valid`.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 05_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// TODO: return std::optional<std::size_t> instead of the "SIZE_MAX means not
// found" convention, which every caller has to know about and none can be
// forced to check.
std::size_t find_index(const std::vector<int>& values, int target) {
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (values[i] == target) {
      return i;
    }
  }
  return static_cast<std::size_t>(-1);
}

// TODO: return std::optional<int> rather than filling in an out-parameter and
// returning a bool.
bool parse_int(std::string_view text, int& out) {
  if (text.empty()) {
    return false;
  }
  int value = 0;
  bool negative = false;
  std::size_t start = 0;
  if (text[0] == '-') {
    if (text.size() == 1) {
      return false;
    }
    negative = true;
    start = 1;
  }
  for (std::size_t i = start; i < text.size(); ++i) {
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
    value = value * 10 + (text[i] - '0');
  }
  out = negative ? -value : value;
  return true;
}

// Returns the first entry that parses as an integer, if any.
//
// TODO: implement it in terms of the new `parse_int`. Note how much smaller
// this gets once the "did it work" and "what is the value" questions have the
// same answer.
std::optional<int> first_valid(const std::vector<std::string>& texts) {
  return std::nullopt;
}

TEST_CASE("find_index says nothing when there is nothing to say") {
  const std::vector<int> values = {10, 20, 30};

  CHECK(find_index(values, 20) == 1);
  CHECK(find_index(values, 10).has_value());
  CHECK_FALSE(find_index(values, 99).has_value());

  // value_or is the reason this reads better than a sentinel: the default is
  // written where it is used, by the caller who knows what it should be.
  CHECK(find_index(values, 99).value_or(0) == 0);
}

TEST_CASE("parse_int returns the value or nothing") {
  CHECK(parse_int("42") == 42);
  CHECK(parse_int("-7") == -7);
  CHECK(parse_int("0") == 0);

  CHECK_FALSE(parse_int("").has_value());
  CHECK_FALSE(parse_int("-").has_value());
  CHECK_FALSE(parse_int("12a").has_value());
  CHECK_FALSE(parse_int("hello").has_value());
}

TEST_CASE("value() throws where operator* would be undefined") {
  const std::optional<int> nothing = parse_int("nope");
  CHECK_THROWS_AS((void)nothing.value(), std::bad_optional_access);
  CHECK(nothing.value_or(-1) == -1);
}

TEST_CASE("first_valid finds the first parseable entry") {
  CHECK(first_valid({"x", "y", "3", "4"}) == 3);
  CHECK(first_valid({"1"}) == 1);
  CHECK_FALSE(first_valid({"a", "b"}).has_value());
  CHECK_FALSE(first_valid({}).has_value());
}

TEST_CASE("an optional costs a bool, not an allocation") {
  static_assert(sizeof(std::optional<int>) <= 2 * sizeof(int));
  static_assert(std::is_trivially_destructible_v<std::optional<int>>);
  CHECK(true);
}
