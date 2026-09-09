// Solution -- 04.01 const correctness
#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <vector>

class Grid {
public:
  Grid(std::size_t rows, std::size_t columns)
      : rows_(rows), columns_(columns), cells_(rows * columns, 0) {}

  [[nodiscard]] std::size_t rows() const {
    return rows_;
  }
  [[nodiscard]] std::size_t columns() const {
    return columns_;
  }
  [[nodiscard]] bool empty() const {
    return cells_.empty();
  }

  [[nodiscard]] int& at(std::size_t row, std::size_t column) {
    return cells_[row * columns_ + column];
  }

  [[nodiscard]] const int& at(std::size_t row, std::size_t column) const {
    return cells_[row * columns_ + column];
  }

  // `mutable` because the cache is not part of the grid's observable value:
  // filling it in does not change what the grid *is*.
  [[nodiscard]] int total() const {
    if (!total_cache_.has_value()) {
      int sum = 0;
      for (const int cell : cells_) {
        sum += cell;
      }
      total_cache_ = sum;
    }
    return *total_cache_;
  }

  void set(std::size_t row, std::size_t column, int value) {
    cells_[row * columns_ + column] = value;
    total_cache_.reset();
  }

private:
  std::size_t rows_;
  std::size_t columns_;
  std::vector<int> cells_;
  mutable std::optional<int> total_cache_;
};

int trace(const Grid& grid) {
  int sum = 0;
  for (std::size_t i = 0; i < grid.rows() && i < grid.columns(); ++i) {
    sum += grid.at(i, i);
  }
  return sum;
}

TEST_CASE("a const Grid can still be read") {
  Grid grid{2, 3};
  grid.set(0, 0, 1);
  grid.set(1, 1, 5);

  const Grid& view = grid;
  CHECK(view.rows() == 2);
  CHECK(view.columns() == 3);
  CHECK_FALSE(view.empty());
  CHECK(view.at(1, 1) == 5);
  CHECK(trace(view) == 6);
}

TEST_CASE("the non-const overload allows writing") {
  Grid grid{2, 2};
  grid.at(0, 1) = 9;
  CHECK(grid.at(0, 1) == 9);
}

TEST_CASE("total is computed lazily but observable through const") {
  Grid grid{2, 2};
  grid.set(0, 0, 3);
  grid.set(1, 1, 4);

  const Grid& view = grid;
  CHECK(view.total() == 7);
  CHECK(view.total() == 7);
}

TEST_CASE("changing a cell invalidates the cache") {
  Grid grid{2, 2};
  grid.set(0, 0, 1);
  CHECK(grid.total() == 1);

  grid.set(0, 1, 10);
  CHECK(grid.total() == 11);
}
