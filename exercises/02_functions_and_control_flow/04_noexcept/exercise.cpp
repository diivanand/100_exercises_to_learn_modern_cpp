// =============================================================================
//  02.04 -- noexcept is a promise, not an optimisation hint
// =============================================================================
//
//  `noexcept` says "this function will not let an exception escape". If one
//  does anyway, the runtime calls std::terminate -- there is no unwinding and
//  no catch. So it is a promise you have to be able to keep.
//
//  Where it genuinely matters:
//
//   * MOVE OPERATIONS. `std::vector` grows by moving its elements into new
//     storage -- but only if the move constructor is `noexcept`. If it might
//     throw, the vector cannot guarantee the strong exception safety it
//     promises, so it *copies* instead. A missing `noexcept` on a move
//     constructor is a silent, whole-program performance bug.
//
//   * SWAP and DESTRUCTORS. Destructors are implicitly noexcept already.
//
//  Where it does not: ordinary functions that might genuinely fail. The
//  rule is "declare `noexcept` when the function cannot or must not throw"
//  (Core Guidelines F.6, E.12), not "sprinkle it everywhere": once it is on
//  a public interface, taking it off again breaks callers who relied on it.
//
//  `noexcept` is also an *operator*: `noexcept(expr)` is a compile-time bool
//  saying whether `expr` can throw. That is how the library detects your move
//  constructor, and how you can write conditional specifications:
//
//      void f() noexcept(noexcept(g()));   // f is noexcept iff g is
//
//  TASK
//    Give `Buffer` a noexcept move constructor and move assignment, and make
//    `swap` noexcept. The tests check the *specification*, not just behaviour.
//
//
//  NOTE  This exercise starts as a compile error. The static_asserts below ask
//        the compiler a question about Buffer's declarations, and right now
//        the answer is no.
//
//  RUN IT
//    ./mcpp test 02_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

class Buffer {
public:
  Buffer() = default;
  explicit Buffer(std::size_t size) : data_(size, 0) {}

  Buffer(const Buffer&) = default;
  Buffer& operator=(const Buffer&) = default;

  // TODO: these two are correct but unmarked, so std::vector will copy Buffers
  // rather than move them when it reallocates. Add `noexcept`.
  //
  // Ask yourself why it is safe: moving a std::vector steals a pointer. No
  // allocation happens, so there is nothing to throw.
  Buffer(Buffer&& other) : data_(std::move(other.data_)) {}

  Buffer& operator=(Buffer&& other) {
    data_ = std::move(other.data_);
    return *this;
  }

  ~Buffer() = default;

  std::size_t size() const noexcept {
    return data_.size();
  }

  // TODO: a member swap can never throw -- it swaps two vectors, which swap
  // pointers. Mark it noexcept.
  void swap(Buffer& other) {
    data_.swap(other.data_);
  }

private:
  std::vector<unsigned char> data_;
};

// TODO: make this noexcept too, so `std::swap` on a Buffer is noexcept.
void swap(Buffer& lhs, Buffer& rhs) {
  lhs.swap(rhs);
}

TEST_CASE("moving a Buffer is declared noexcept") {
  // These are compile-time queries about the *declaration*. They are what the
  // standard library itself checks.
  static_assert(std::is_nothrow_move_constructible_v<Buffer>);
  static_assert(std::is_nothrow_move_assignable_v<Buffer>);
  static_assert(noexcept(std::declval<Buffer&>().swap(std::declval<Buffer&>())));
  CHECK(true);
}

TEST_CASE("a vector of Buffers moves rather than copies when it grows") {
  // std::vector only chooses the move path when the move constructor is
  // noexcept; otherwise it must copy to keep its strong guarantee.
  static_assert(
      std::is_same_v<decltype(std::move_if_noexcept(std::declval<Buffer&>())), Buffer&&>);
  CHECK(true);
}

TEST_CASE("moving still does the right thing at run time") {
  Buffer source{16};
  Buffer target{std::move(source)};
  CHECK(target.size() == 16);
  // A moved-from std::vector is guaranteed to be valid; libc++ leaves it empty.
  CHECK(source.size() == 0);

  Buffer a{4};
  Buffer b{9};
  swap(a, b);
  CHECK(a.size() == 9);
  CHECK(b.size() == 4);
}
