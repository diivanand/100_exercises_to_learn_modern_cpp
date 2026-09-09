// Solution -- 03.01 Value semantics
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

class Matrix {
public:
  Matrix(std::size_t rows, std::size_t columns)
      : rows_(rows), columns_(columns), cells_(new double[rows * columns]{}) {}

  Matrix(const Matrix& other)
      : rows_(other.rows_), columns_(other.columns_),
        cells_(new double[other.rows_ * other.columns_]) {
    std::copy_n(other.cells_, rows_ * columns_, cells_);
  }

  // Copy-and-swap: the parameter is constructed with the copy constructor
  // (which may throw, before we have touched *this*), then swapped in. Self
  // assignment works because `other` is already an independent copy, and the
  // strong exception guarantee falls out for free.
  Matrix& operator=(Matrix other) noexcept {
    swap(other);
    return *this;
  }

  Matrix(Matrix&& other) noexcept
      : rows_(std::exchange(other.rows_, 0)), columns_(std::exchange(other.columns_, 0)),
        cells_(std::exchange(other.cells_, nullptr)) {}

  ~Matrix() {
    delete[] cells_;
  }

  [[nodiscard]] std::size_t rows() const noexcept {
    return rows_;
  }
  [[nodiscard]] std::size_t columns() const noexcept {
    return columns_;
  }

  [[nodiscard]] double& at(std::size_t row, std::size_t column) {
    return cells_[row * columns_ + column];
  }
  [[nodiscard]] double at(std::size_t row, std::size_t column) const {
    return cells_[row * columns_ + column];
  }

  void swap(Matrix& other) noexcept {
    std::swap(rows_, other.rows_);
    std::swap(columns_, other.columns_);
    std::swap(cells_, other.cells_);
  }

private:
  std::size_t rows_ = 0;
  std::size_t columns_ = 0;
  double* cells_ = nullptr;
};

TEST_CASE("a copy is independent of its source") {
  Matrix original{2, 2};
  original.at(0, 0) = 1.0;

  Matrix copy = original;
  copy.at(0, 0) = 99.0;

  CHECK(original.at(0, 0) == doctest::Approx(1.0));
  CHECK(copy.at(0, 0) == doctest::Approx(99.0));
}

TEST_CASE("assignment replaces the target's contents") {
  Matrix a{2, 2};
  a.at(1, 1) = 5.0;

  Matrix b{3, 3};
  b = a;

  CHECK(b.rows() == 2);
  CHECK(b.at(1, 1) == doctest::Approx(5.0));

  a.at(1, 1) = 7.0;
  CHECK(b.at(1, 1) == doctest::Approx(5.0));
}

TEST_CASE("self-assignment is harmless") {
  Matrix m{2, 2};
  m.at(0, 1) = 3.0;

  Matrix& alias = m;
  m = alias;

  CHECK(m.at(0, 1) == doctest::Approx(3.0));
}

TEST_CASE("destroying a copy does not disturb the original") {
  Matrix original{2, 2};
  original.at(0, 0) = 42.0;
  {
    const Matrix copy = original;
    CHECK(copy.at(0, 0) == doctest::Approx(42.0));
  }
  CHECK(original.at(0, 0) == doctest::Approx(42.0));
}
