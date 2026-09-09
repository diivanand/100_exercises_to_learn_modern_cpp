// Solution -- 06.02 How a vector grows
#include <doctest/doctest.h>

#include <cstddef>
#include <vector>

std::vector<int> build(std::size_t count) {
  std::vector<int> result;
  result.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    result.push_back(static_cast<int>(i * i));
  }
  return result;
}

int& append_pair(std::vector<int>& values, int a, int b) {
  const std::size_t index = values.size();
  values.push_back(a);
  values.push_back(b);
  // The index is still valid after any number of reallocations; a reference
  // taken before the second push would not have been.
  return values[index];
}

void amplify(std::vector<int>& values, int threshold) {
  // Indices survive reallocation; iterators do not. The original size is
  // captured first so the loop does not walk over the markers it appends.
  const std::size_t original_size = values.size();
  for (std::size_t i = 0; i < original_size; ++i) {
    if (values[i] > threshold) {
      values[i] *= 2;
      values.push_back(-1);
    }
  }
}

TEST_CASE("build allocates once") {
  const std::size_t count = 1000;
  const std::vector<int> values = build(count);

  CHECK(values.size() == count);
  CHECK(values[3] == 9);
  CHECK(values.capacity() == count);
}

TEST_CASE("append_pair survives the reallocation between the two pushes") {
  std::vector<int> values;
  values.reserve(1);

  int& first = append_pair(values, 10, 20);
  CHECK(first == 10);

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

  const std::size_t capacity_before = resized.capacity();
  resized.clear();
  CHECK(resized.empty());
  CHECK(resized.capacity() == capacity_before);
}
