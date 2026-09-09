// Solution -- 06.09 Erasing elements
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

void remove_all(std::vector<int>& values, int value) {
  // C++20's std::erase is the erase-remove idiom with a name, and it returns
  // the number of elements removed.
  std::erase(values, value);
}

void remove_short(std::vector<std::string>& words, std::size_t minimum) {
  std::erase_if(words,
                [minimum](const std::string& word) { return word.size() < minimum; });
}

std::size_t drop_expired(std::map<std::string, int>& leases) {
  // Written out longhand, because this loop is worth being able to write: the
  // iterator returned by erase is the one to continue from.
  std::size_t removed = 0;
  for (auto it = leases.begin(); it != leases.end();) {
    if (it->second <= 0) {
      it = leases.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }
  return removed;

  // The one-line version:
  //   return std::erase_if(leases, [](const auto& entry) {
  //     return entry.second <= 0;
  //   });
}

TEST_CASE("remove_all actually removes") {
  std::vector<int> values = {1, 2, 3, 2, 4, 2};
  remove_all(values, 2);

  CHECK(values == std::vector<int>{1, 3, 4});
  CHECK(values.size() == 3);
}

TEST_CASE("removing something that is not there changes nothing") {
  std::vector<int> values = {1, 2, 3};
  remove_all(values, 99);
  CHECK(values == std::vector<int>{1, 2, 3});
}

TEST_CASE("remove_short keeps the order of what remains") {
  std::vector<std::string> words = {"a", "four", "to", "seven!!", "no"};
  remove_short(words, 4);
  CHECK(words == std::vector<std::string>{"four", "seven!!"});
}

TEST_CASE("consecutive matches are all removed") {
  std::vector<std::string> words = {"ok!!!", "a", "b", "c", "fine!"};
  remove_short(words, 4);
  CHECK(words == std::vector<std::string>{"ok!!!", "fine!"});
}

TEST_CASE("drop_expired removes every expired lease") {
  std::map<std::string, int> leases = {{"a", 5}, {"b", 0}, {"c", -1}, {"d", 3}, {"e", 0}};

  CHECK(drop_expired(leases) == 3);
  CHECK(leases.size() == 2);
  CHECK(leases.contains("a"));
  CHECK(leases.contains("d"));
  CHECK_FALSE(leases.contains("b"));
}

TEST_CASE("dropping from an empty or all-expired map") {
  std::map<std::string, int> empty;
  CHECK(drop_expired(empty) == 0);

  std::map<std::string, int> all_expired = {{"a", 0}, {"b", 0}};
  CHECK(drop_expired(all_expired) == 2);
  CHECK(all_expired.empty());
}
