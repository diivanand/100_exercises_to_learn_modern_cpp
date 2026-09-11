// =============================================================================
//  02.03 -- [[nodiscard]] and friends
// =============================================================================
//
//  Attributes let you tell the compiler things the type system cannot express.
//  Three of them earn their keep every day:
//
//    [[nodiscard]]      ignoring this return value is a bug. On a function
//                       that has no other effect, dropping the result means
//                       the call did nothing. On one that returns an error,
//                       dropping it means the error vanished.
//
//    [[maybe_unused]]   this really is unused in some configuration; stop
//                       warning about it. Better than `(void)x;`.
//
//    [[fallthrough]]    this switch case falls into the next one on purpose.
//                       Without it, -Wimplicit-fallthrough flags every case
//                       that does not break -- and finds the real ones.
//
//  C++20 lets `[[nodiscard]]` carry a reason: `[[nodiscard("check this")]]`.
//  It can also go on a *type*, which marks every function returning it.
//
//  TASK
//    Mark the functions below so the compiler catches the misuse in the
//    commented-out lines, and fix the switch so `classify` is right.
//
//  RUN IT
//    ./mcpp test 02_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// A pure computation: ignoring the result means the call was pointless.
// TODO: add [[nodiscard]].
std::vector<int> sorted_copy(std::vector<int> values) {
  // Passing by value and sorting the parameter is deliberate -- see 02.06.
  for (std::size_t i = 1; i < values.size(); ++i) {
    for (std::size_t j = i; j > 0 && values[j - 1] > values[j]; --j) {
      const int tmp = values[j - 1];
      values[j - 1] = values[j];
      values[j] = tmp;
    }
  }
  return values;
}

// An error code. Silently dropping one is how "it failed but nothing
// happened" bugs are born.
// TODO: mark the *type* [[nodiscard]] with a reason, so every function
// returning a ParseResult is covered without repeating the attribute.
enum class ParseResult { kOk, kEmpty, kNotANumber };

ParseResult parse_int(std::string_view text, int& out) {
  if (text.empty()) {
    return ParseResult::kEmpty;
  }
  int value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') {
      return ParseResult::kNotANumber;
    }
    value = value * 10 + (c - '0');
  }
  out = value;
  return ParseResult::kOk;
}

// TODO: `kEmpty` and `kNotANumber` should both report "bad input", but the
// empty case should say "empty: " first and then fall into the shared text.
// Write that with a [[fallthrough]], so the fallthrough is explicit rather
// than accidental.
std::string describe(ParseResult result, [[maybe_unused]] std::string_view input) {
  switch (result) {
  case ParseResult::kOk:
    return "ok";
  case ParseResult::kEmpty:
  case ParseResult::kNotANumber:
    return "bad input";
  }
  return "unreachable";
}

TEST_CASE("sorted_copy leaves the original alone") {
  const std::vector<int> original = {3, 1, 2};
  CHECK(sorted_copy(original) == std::vector<int>{1, 2, 3});
  CHECK(original == std::vector<int>{3, 1, 2});

  // With [[nodiscard]] in place, this line stops compiling. Uncomment it to
  // see the diagnostic:
  //
  //   sorted_copy(original);
}

TEST_CASE("parse_int reports why it failed") {
  int value = -1;
  CHECK(parse_int("123", value) == ParseResult::kOk);
  CHECK(value == 123);
  CHECK(parse_int("", value) == ParseResult::kEmpty);
  CHECK(parse_int("12a", value) == ParseResult::kNotANumber);

  // With [[nodiscard]] on the type, dropping the result stops compiling:
  //
  //   parse_int("123", value);
}

TEST_CASE("describe distinguishes the two failures") {
  CHECK(describe(ParseResult::kOk, "1") == "ok");
  CHECK(describe(ParseResult::kEmpty, "") == "empty: bad input");
  CHECK(describe(ParseResult::kNotANumber, "x") == "bad input");
}
