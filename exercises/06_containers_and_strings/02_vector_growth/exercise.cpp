// =============================================================================
//  06.02 -- How a vector grows, and what that invalidates
// =============================================================================
//
//  `std::vector` is the default container (Core Guidelines SL.con.2). Knowing
//  two things about it explains most of the surprises.
//
//  1. GROWTH IS AMORTISED, BY REALLOCATION. When `size() == capacity()` and
//     you push, the vector allocates a bigger block (typically 1.5x or 2x),
//     moves every element across, and frees the old one. Each push is O(1) on
//     average; the individual push that reallocates is O(n).
//
//     `reserve(n)` does that once, up front, when you know the size. It sets
//     capacity, NOT size -- `reserve(10)` leaves the vector empty. `resize(10)`
//     creates ten elements.
//
//  2. REALLOCATION INVALIDATES EVERYTHING. Every iterator, pointer and
//     reference into the vector points at the old block, which has just been
//     freed. This is the single most common source of use-after-free in C++:
//
//         auto& first = v[0];
//         v.push_back(x);      // may reallocate
//         first = 1;           // use-after-free, silently
//
//     `erase` and `insert` invalidate from the modified position onward.
//     `clear()` invalidates everything but keeps the capacity.
//
//  TASK
//    Fix the two dangling-reference bugs, and make `build` allocate exactly
//    once.
//
//  RUN IT
//    ./mcpp test 06_02
//
//  THEN
//    Re-run it under `cmake --preset asan && ctest --preset asan -R 06_02`.
//    A use-after-free that "works" is still a use-after-free.
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

// Builds a vector of `count` squares.
//
// TODO: reserve the capacity up front. As written this reallocates roughly
// log2(count) times, moving every element each time.
std::vector<int> build(std::size_t count) {
  std::vector<int> result;
  for (std::size_t i = 0; i < count; ++i) {
    result.push_back(static_cast<int>(i * i));
  }
  return result;
}

// Appends both values and returns a reference to the first one added.
//
// TODO: `first_added` is captured before the second push. If that push
// reallocates, the reference points into the freed block -- and returning it
// hands the caller a dangling reference that will often *appear* to work.
//
// Fix it by remembering the index instead: indices survive reallocation,
// references do not.
int& append_pair(std::vector<int>& values, int a, int b) {
  values.push_back(a);
  int& first_added = values.back();
  values.push_back(b);
  return first_added;
}

// Doubles every element whose value exceeds `threshold`, appending a marker
// for each one.
//
// TODO: this iterates while pushing, so the iterator is invalidated the first
// time the vector reallocates. Collect what to append first, then append --
// or work with indices, which survive reallocation.
void amplify(std::vector<int>& values, int threshold) {
  for (auto it = values.begin(); it != values.end(); ++it) {
    if (*it > threshold) {
      *it *= 2;
      values.push_back(-1);
    }
  }
}

TEST_CASE("build allocates once") {
  const std::size_t count = 1000;
  const std::vector<int> values = build(count);

  CHECK(values.size() == count);
  CHECK(values[3] == 9);

  // A vector that reserved exactly what it needed has capacity == size. One
  // that grew geometrically will have overshot.
  CHECK(values.capacity() == count);
}

TEST_CASE("append_pair survives the reallocation between the two pushes") {
  std::vector<int> values;
  // Capacity for exactly one element, so the second push must reallocate.
  values.reserve(1);

  int& first = append_pair(values, 10, 20);
  CHECK(first == 10);

  // Writing through the returned reference must reach the live vector.
  first = 11;
  CHECK(values.front() == 11);
  CHECK(values.back() == 20);
  CHECK(values.size() == 2);
}

TEST_CASE("amplify survives the reallocation it causes") {
  std::vector<int> values = {1, 5, 2, 7};
  amplify(values, 4);

  CHECK(values.size() == 6);
  CHECK(values[0] == 1);
  CHECK(values[1] == 10);
  CHECK(values[2] == 2);
  CHECK(values[3] == 14);
  CHECK(values[4] == -1);
  CHECK(values[5] == -1);
}

TEST_CASE("reserve sets capacity, resize sets size") {
  std::vector<int> reserved;
  reserved.reserve(10);
  CHECK(reserved.size() == 0);
  CHECK(reserved.capacity() >= 10);

  std::vector<int> resized;
  resized.resize(10);
  CHECK(resized.size() == 10);
  CHECK(resized[0] == 0);

  // clear() throws away the elements but keeps the buffer, which is why
  // reusing a vector in a loop beats creating a new one.
  const std::size_t capacity_before = resized.capacity();
  resized.clear();
  CHECK(resized.empty());
  CHECK(resized.capacity() == capacity_before);
}
