// =============================================================================
//  06.05 -- std::span (C++20)
// =============================================================================
//
//  `std::span<T>` is to sequences what `std::string_view` is to strings: a
//  pointer and a length, referring to contiguous elements it does not own.
//
//  It replaces the (pointer, count) pair that C forced on you, and it replaces
//  the template-on-container that C++ pushed you toward:
//
//      void process(const int* data, std::size_t count);      // C
//      template <typename C> void process(const C& container); // C++03..17
//      void process(std::span<const int> data);                // C++20
//
//  One non-template signature, in a header, that accepts a std::vector, a
//  std::array, a C array and a subrange of any of them -- and carries the
//  length so it cannot be lied about.
//
//  `std::span<const T>` means "I will read it"; `std::span<T>` means "I may
//  write through it". The span itself is always cheap to copy -- constness of
//  the span and constness of the elements are separate things.
//
//  It has the same lifetime hazard as string_view: a span into a vector is
//  invalidated by anything that reallocates that vector (06.02).
//
//  `subspan`, `first` and `last` slice without copying.
//
//  TASK
//    Replace the pointer-and-count and container-template signatures with
//    spans, and implement `chunk_sums` using subspan.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 06_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

// TODO: take a std::span<const int>. The caller currently has to pass a count
// that matches the pointer, and nothing checks that it does.
int sum(const int* values, std::size_t count) {
  int total = 0;
  for (std::size_t i = 0; i < count; ++i) {
    total += values[i];
  }
  return total;
}

// TODO: take a std::span<int> -- a mutable view. This works today only because
// it is a template, which means it lives in a header and is recompiled for
// every container type it meets.
template <typename Container>
void scale(Container& values, int factor) {
  for (auto& value : values) {
    value *= factor;
  }
}

// Sums each consecutive group of `chunk_size` elements. The final group may be
// shorter.
//
// TODO: implement with `subspan`, which slices without copying anything.
std::vector<int> chunk_sums(std::span<const int> values, std::size_t chunk_size) {
  return {};
}

TEST_CASE("one signature accepts every contiguous container") {
  const std::vector<int> vector = {1, 2, 3};
  const std::array<int, 3> array = {4, 5, 6};
  // A raw C array, deliberately: the point of this test is that ONE span
  // signature accepts it alongside a vector and a std::array.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
  const int c_array[3] = {7, 8, 9};

  CHECK(sum(vector) == 6);
  CHECK(sum(array) == 15);
  CHECK(sum(c_array) == 24);
  CHECK(sum(std::vector<int>{}) == 0);
}

TEST_CASE("a mutable span writes through to the original") {
  std::vector<int> values = {1, 2, 3};
  scale(values, 10);
  CHECK(values == std::vector<int>{10, 20, 30});

  std::array<int, 2> array = {1, 2};
  scale(array, 3);
  CHECK(array == std::array<int, 2>{3, 6});

  // A span over part of a container: only those elements change.
  std::vector<int> partial = {1, 1, 1, 1};
  scale(std::span{partial}.subspan(1, 2), 5);
  CHECK(partial == std::vector<int>{1, 5, 5, 1});
}

TEST_CASE("chunk_sums slices without copying") {
  const std::vector<int> values = {1, 2, 3, 4, 5};

  CHECK(chunk_sums(values, 2) == std::vector<int>{3, 7, 5});
  CHECK(chunk_sums(values, 5) == std::vector<int>{15});
  CHECK(chunk_sums(values, 10) == std::vector<int>{15});
  CHECK(chunk_sums({}, 3).empty());
}

TEST_CASE("a span is a pointer and a length") {
  static_assert(sizeof(std::span<int>) == 2 * sizeof(void*));

  // A span with a compile-time extent is just the pointer.
  static_assert(sizeof(std::span<int, 3>) == sizeof(void*));
  CHECK(true);
}
