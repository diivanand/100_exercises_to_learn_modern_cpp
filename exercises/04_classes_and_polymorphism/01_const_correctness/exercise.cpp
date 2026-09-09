// =============================================================================
//  04.01 -- const correctness
// =============================================================================
//
//  `const` is the cheapest documentation in the language, and the only kind
//  the compiler checks. Applied consistently it tells a reader which functions
//  can change an object and which cannot -- and then enforces it.
//
//  On a member function, `const` binds to `*this`:
//
//      int  size() const;   // callable on a const Foo; cannot modify members
//      void clear();        // not callable on a const Foo
//
//  Consequences worth knowing:
//
//   * A const object can only have its const member functions called. Miss one
//     and callers are forced to give up const somewhere -- const-correctness
//     is viral, in the good direction.
//
//   * An accessor usually needs TWO overloads: one const returning `const T&`,
//     one non-const returning `T&`. That is not duplication for its own sake;
//     it is the type system carrying the caller's intent through.
//
//   * `mutable` opts one member out, for state that is not part of the
//     object's observable value: a cache, a mutex, a hit counter. Use it for
//     exactly that and nothing else.
//
//   * const is about the *interface*, not about optimisation. `const T&`
//     parameters are for saying "I will not modify this" (Core Guidelines
//     Con.1-Con.4).
//
//  TASK
//    Make `Grid` const-correct: every observer const, a mutable cache for the
//    lazily computed total, and both flavours of `at`.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 04_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <vector>

class Grid {
public:
  Grid(std::size_t rows, std::size_t columns)
      : rows_(rows), columns_(columns), cells_(rows * columns, 0) {}

  // TODO: these three observe and do not modify. Make them const.
  [[nodiscard]] std::size_t rows() {
    return rows_;
  }
  [[nodiscard]] std::size_t columns() {
    return columns_;
  }
  [[nodiscard]] bool empty() {
    return cells_.empty();
  }

  // TODO: provide both overloads:
  //   int& at(row, column)              -- for writing
  //   const int& at(row, column) const  -- for reading through a const Grid
  [[nodiscard]] int& at(std::size_t row, std::size_t column) {
    return cells_[row * columns_ + column];
  }

  // Sums every cell. The sum is expensive enough to be worth caching, and the
  // cache is not part of the grid's value -- two grids with the same cells are
  // equal whether or not either has computed its total.
  //
  // TODO: make this const, and make `total_cache_` mutable so it can still be
  // filled in. Remember to invalidate the cache in `set`.
  [[nodiscard]] int total() {
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
  }

private:
  std::size_t rows_;
  std::size_t columns_;
  std::vector<int> cells_;
  std::optional<int> total_cache_;
};

// Reads a grid it has promised not to change. Everything it calls must be
// const, which is how one missing `const` propagates outward until somebody
// gives up and casts it away.
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
  // Cached: the second call must give the same answer.
  CHECK(view.total() == 7);
}

TEST_CASE("changing a cell invalidates the cache") {
  Grid grid{2, 2};
  grid.set(0, 0, 1);
  CHECK(grid.total() == 1);

  grid.set(0, 1, 10);
  CHECK(grid.total() == 11);
}
