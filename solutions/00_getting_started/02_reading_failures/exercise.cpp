// Solution -- 00.02 Reading a failure
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

  // Convert *before* dividing, so the division is done in double. The cast is
  // explicit and named: `static_cast` is greppable in a way a C-style cast is
  // not (Core Guidelines ES.49).
  return static_cast<double>(total) / static_cast<double>(values.size());
}

std::size_t count_long_words(const std::vector<std::string_view>& words,
                             std::size_t min_length) {
  std::size_t count = 0;
  for (const std::string_view word : words) {
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
