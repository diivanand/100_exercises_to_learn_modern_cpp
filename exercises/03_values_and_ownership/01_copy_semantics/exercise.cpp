// =============================================================================
//  03.01 -- Value semantics
// =============================================================================
//
//  In C++ a variable *is* an object, not a handle to one. `a = b` copies; two
//  names never refer to the same thing unless you asked for a reference or a
//  pointer. This is the language's central design decision, and almost
//  everything else follows from it -- destructors that run at a known time,
//  containers that own their elements, RAII, no garbage collector.
//
//  The consequence you must internalise: a copy is INDEPENDENT. If modifying
//  one object changes another, you have written a bug, and the usual cause is
//  a class that copies a pointer instead of what it points at (a "shallow
//  copy").
//
//  The compiler will write copy operations for you, member by member. For a
//  class holding only values (std::string, std::vector, int) that is exactly
//  right. For a class holding a raw pointer it is exactly wrong -- see 03.05.
//
//  TASK
//    `Matrix` below shares its storage between copies, so changing one changes
//    the other, and destroying one leaves the other dangling. Give it real
//    value semantics.
//
//  NOTE  This exercise compiles as it stands: the compiler generates the
//        copy operations, and they copy the pointer. Expect it to fail its
//        tests and then crash with a double free. Run it under `asan` to see
//        the bug named precisely.
//
//  RUN IT
//    ./mcpp test 03_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <vector>

class Matrix {
public:
  Matrix(std::size_t rows, std::size_t columns)
      : rows_(rows), columns_(columns), cells_(new double[rows * columns]{}) {}

  // TODO: this class owns a raw `new[]` allocation, so the compiler-generated
  // copy operations are wrong -- they copy the *pointer*. Write a copy
  // constructor and a copy assignment operator that copy the cells, and a
  // destructor that frees them.
  //
  // Write them by hand here so you can see the work; 03.04 shows how to make
  // the compiler do it for you, which is what you should actually ship.
  //
  // For copy assignment, remember the two hazards:
  //   * self-assignment (`m = m;`) must not free the storage it is about to
  //     read from;
  //   * if the new allocation throws, the object must be left as it was.
  //
  // The copy-and-swap idiom handles both:
  //   Matrix& operator=(Matrix other) { swap(other); return *this; }

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
  // If the copy shared the original's storage, the read below would be a
  // use-after-free. Run this exercise under `cmake --preset asan` to see the
  // difference for yourself.
  CHECK(original.at(0, 0) == doctest::Approx(42.0));
}
