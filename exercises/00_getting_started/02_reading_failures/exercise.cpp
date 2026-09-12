// =============================================================================
//  00.02 -- Reading a failure, and taking warnings seriously
// =============================================================================
//
//  Two habits worth building before you write any real C++.
//
//  1. READ THE FAILURE.
//     doctest prints the *expression* and the *values* it saw, not just
//     "assertion failed":
//
//         exercise.cpp:57: ERROR: CHECK( average(a) == 3.0 ) is NOT correct!
//           values: CHECK( 2.5 == 3.0 )
//
//     That second line tells you the bug is in the arithmetic, not in the
//     test. Most of debugging is noticing which number is wrong.
//
//  2. TREAT WARNINGS AS ERRORS.
//     This project builds with -Wall -Wextra -Wconversion -Wsign-conversion
//     ... -Werror. That is deliberate: it turns a whole class of silent bugs
//     into build failures. Not every silent bug is a warning, though.
//     `average` below contains the classic one -- an integer division that
//     quietly throws away the fractional part -- and no compiler flags it,
//     because every step is a legal int operation. Only the test does.
//
//     (If you ever need to build despite a warning, configure with
//      -DMCPP_WARNINGS_AS_ERRORS=OFF. Use it to keep moving, not as a habit.)
//
//  TASK
//    Fix `average` so it computes a real average, and fix the off-by-one in
//    `count_long_words`.
//
//  RUN IT
//    ./mcpp test 00_02
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <string_view>
#include <vector>

double average(const std::vector<int>& values) {
  if (values.empty()) {
    return 0.0;
  }

  int total = 0;
  for (const int value : values) {
    total += value;
  }

  // TODO: this is integer division -- 5 / 2 is 2, not 2.5. Both operands are
  // ints, so the division happens *before* the result is widened to double.
  // Convert deliberately, with a named cast, before dividing.
  return total / static_cast<double>(values.size());
}

// `words` is read-only, so it is a view of const strings.
std::size_t count_long_words(const std::vector<std::string_view>& words,
                             std::size_t min_length) {
  std::size_t count = 0;
  for (const std::string_view word : words) {
    // TODO: "long" means *at least* `min_length` characters. This is off by
    // one, and no compiler can tell you so -- which is exactly why the tests
    // exist. Warnings catch the mechanical mistakes; tests catch the ones you
    // make about meaning.
    if (word.size() >= min_length) {
      ++count;
    }
  }
  return count;
}

TEST_CASE("average of integers is not an integer") {
  CHECK(average({1, 2, 3, 4}) == doctest::Approx(2.5));
  CHECK(average({5, 2}) == doctest::Approx(3.5));
  CHECK(average({}) == doctest::Approx(0.0));
}

TEST_CASE("count_long_words counts words of at least min_length") {
  const std::vector<std::string_view> words = {"a", "bb", "ccc", "dddd"};
  CHECK(count_long_words(words, 3) == 2);
  CHECK(count_long_words(words, 1) == 4);
  CHECK(count_long_words(words, 5) == 0);
}
