// Solution -- 06.05 std::span
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

int sum(std::span<const int> values) {
  int total = 0;
  for (const int value : values) {
    total += value;
  }
  return total;
}

void scale(std::span<int> values, int factor) {
  for (int& value : values) {
    value *= factor;
  }
}

std::vector<int> chunk_sums(std::span<const int> values, std::size_t chunk_size) {
  std::vector<int> sums;
  if (chunk_size == 0) {
    return sums;
  }

  while (!values.empty()) {
    const std::size_t take = std::min(chunk_size, values.size());
    sums.push_back(sum(values.first(take)));
    values = values.subspan(take);
  }
  return sums;
}

TEST_CASE("one signature accepts every contiguous container") {
  const std::vector<int> vector = {1, 2, 3};
  const std::array<int, 3> array = {4, 5, 6};
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
  static_assert(sizeof(std::span<int, 3>) == sizeof(void*));
  CHECK(true);
}
