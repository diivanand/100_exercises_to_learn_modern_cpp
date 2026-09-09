// Solution -- 01.01 auto
#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <vector>

void shout(std::vector<std::string>& words) {
  for (auto& word : words) {
    word += '!';
  }
}

std::string_view longest(const std::vector<std::string>& words) {
  std::string_view best;
  for (const auto& word : words) {
    if (word.size() > best.size()) {
      best = word;
    }
  }
  return best;
}

TEST_CASE("shout modifies the vector in place") {
  std::vector<std::string> words = {"go", "fast"};
  shout(words);
  CHECK(words == std::vector<std::string>{"go!", "fast!"});
}

TEST_CASE("longest returns a view into the original strings") {
  const std::vector<std::string> words = {"a", "three", "of", "elephant", "cat"};
  CHECK(longest(words) == "elephant");
  CHECK(longest(words).data() == words[3].data());
}

TEST_CASE("longest handles the empty case") {
  const std::vector<std::string> none;
  CHECK(longest(none).empty());
}
