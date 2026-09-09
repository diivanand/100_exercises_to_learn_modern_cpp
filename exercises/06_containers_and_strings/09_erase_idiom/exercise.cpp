// =============================================================================
//  06.09 -- Erasing elements
// =============================================================================
//
//  Removing elements from a container is the operation people most often get
//  wrong, because the obvious code is a use-after-free.
//
//  THE ERASE-REMOVE IDIOM, for sequence containers. `std::remove` does not
//  remove anything -- it cannot, it only has iterators. It shuffles the
//  elements you are keeping to the front and returns an iterator to the new
//  logical end. `erase` then throws away the tail:
//
//      v.erase(std::remove(v.begin(), v.end(), value), v.end());
//
//  Forget the `erase` and the container still has its old size, with junk at
//  the back. That mistake is common enough that `std::remove` is [[nodiscard]]
//  in C++20.
//
//  C++20 FINALLY GAVE IT A NAME: `std::erase(v, value)` and
//  `std::erase_if(v, predicate)`. They work on every standard container,
//  return how many elements went, and cannot be half-written. Use them.
//
//  ERASING WHILE ITERATING. `erase` returns an iterator to the element AFTER
//  the one removed. That is what makes this loop correct:
//
//      for (auto it = m.begin(); it != m.end(); ) {
//        if (drop(*it)) { it = m.erase(it); } else { ++it; }
//      }
//
//  Incrementing an erased iterator instead is undefined behaviour.
//
//  TASK
//    Fix the three removal bugs below.
//
//  RUN IT
//    ./mcpp test 06_09
//
//  THEN
//    Run it under `cmake --preset asan` -- `drop_expired` is a use-after-free
//    that happens to produce the right answer on small inputs.
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <tuple>
#include <vector>

// TODO: this calls std::remove and throws the result away, so the vector still
// contains the same number of elements -- the unwanted ones have merely been
// shuffled to the back. Use std::erase.
void remove_all(std::vector<int>& values, int value) {
  std::ignore = std::remove(values.begin(), values.end(), value);
}

// Removes every string shorter than `minimum`.
//
// TODO: use std::erase_if. The hand-written loop below erases while iterating
// forward and then increments the invalidated iterator.
void remove_short(std::vector<std::string>& words, std::size_t minimum) {
  for (auto it = words.begin(); it != words.end(); ++it) {
    if (it->size() < minimum) {
      words.erase(it);
    }
  }
}

// Removes every entry whose value has expired (is <= 0), returning how many
// went.
//
// TODO: fix the iteration. `erase` invalidates the iterator it was given;
// using its return value is the whole technique. std::erase_if works here too
// and is shorter -- use whichever you prefer, but make sure you can write both.
std::size_t drop_expired(std::map<std::string, int>& leases) {
  std::size_t removed = 0;
  for (auto it = leases.begin(); it != leases.end(); ++it) {
    if (it->second <= 0) {
      leases.erase(it);
      ++removed;
    }
  }
  return removed;
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
  // This is the case a forward-erasing loop skips: after erasing the element
  // at position i, the next element slides into position i, and ++it steps
  // straight over it.
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
