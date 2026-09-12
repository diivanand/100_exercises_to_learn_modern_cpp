// =============================================================================
//  05.08 -- Invariants, preconditions, and where to check them
// =============================================================================
//
//  Three different kinds of "this must be true", with three different answers.
//
//    INVARIANT      always true of an object between operations.
//                   Established by the constructor, preserved by every public
//                   member function. If a class has no invariant, it should
//                   probably be a struct with public members.
//
//    PRECONDITION   what a function requires of its caller. Violating one is a
//                   BUG IN THE CALLER.
//
//    POSTCONDITION  what a function promises. Violating one is a bug in the
//                   function.
//
//  How to enforce them:
//
//   * `assert` for bugs. It documents the assumption, checks it in debug
//     builds, and costs nothing in release (NDEBUG). Use it for conditions
//     that a correct program cannot violate.
//
//   * `throw` for bad input. If the value came from a user, a file or the
//     network, it is not a bug -- it is data, and the caller deserves an
//     error, not a crash.
//
//   * `static_assert` when the condition is about types or constants. Free,
//     and impossible to skip.
//
//  The distinction matters: `assert` on user input turns a bad request into a
//  crash in debug and undefined behaviour in release. C++20 has no contracts,
//  so this judgement is yours to make (Core Guidelines I.6, I.8, C.2).
//
//  TASK
//    `Percentage` and `RingBuffer` have their checks in the wrong places. Move
//    each one to the right kind, and establish RingBuffer's invariant in its
//    constructor rather than hoping.
//
//  RUN IT
//    ./mcpp test 05_08
//
// =============================================================================

#include <doctest/doctest.h>

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// A percentage is a number in 0..100. That is its invariant, and it is why
// this is a class rather than a bare int.
class Percentage {
public:
  // TODO: this value comes from parsing text -- external input, so bad values
  // are data, not bugs. Throw std::out_of_range rather than asserting.
  explicit Percentage(int value) : value_(value) {
    assert(value >= 0 && value <= 100);
  }

  int value() const noexcept {
    return value_;
  }

  // TODO: `other` is a Percentage, so it is already in range -- that is the
  // invariant doing its job. What can go wrong here is the SUM exceeding 100,
  // which is again a caller error rather than bad data... but this function is
  // used to combine user-supplied values, so it should throw too.
  Percentage operator+(const Percentage& other) const {
    return Percentage{value_ + other.value_};
  }

private:
  int value_;
};

// A fixed-capacity circular buffer.
class RingBuffer {
public:
  // TODO: a capacity of zero makes every other operation meaningless -- the
  // class could not maintain its invariant. This is a programming error at the
  // point of construction: assert it, and document why.
  //
  // (You may reasonably disagree and prefer a throw here. Say why in a comment
  // if you do -- the judgement is the exercise.)
  explicit RingBuffer(std::size_t capacity) : storage_(capacity) {}

  void push(int value) {
    // TODO: overwriting the oldest element when full is this buffer's defined
    // behaviour, not an error. No check belongs here at all.
    if (size_ == storage_.size()) {
      throw std::runtime_error{"buffer full"};
    }
    storage_[(head_ + size_) % storage_.size()] = value;
    ++size_;
  }

  // TODO: calling `front()` on an empty buffer is a precondition violation --
  // there is no correct value to return, and no way for the function to
  // recover. Assert it.
  int front() const {
    if (size_ == 0) {
      return 0;
    }
    return storage_[head_];
  }

  void pop() {
    assert(size_ > 0 && "pop() on an empty buffer");
    head_ = (head_ + 1) % storage_.size();
    --size_;
  }

  std::size_t size() const noexcept {
    return size_;
  }
  std::size_t capacity() const noexcept {
    return storage_.size();
  }
  bool full() const noexcept {
    return size_ == storage_.size();
  }

private:
  std::vector<int> storage_;
  std::size_t head_ = 0;
  std::size_t size_ = 0;
};

TEST_CASE("a bad percentage is an error, not a crash") {
  CHECK(Percentage{50}.value() == 50);
  CHECK(Percentage{0}.value() == 0);
  CHECK(Percentage{100}.value() == 100);

  CHECK_THROWS_AS(Percentage{-1}, std::out_of_range);
  CHECK_THROWS_AS(Percentage{101}, std::out_of_range);
}

TEST_CASE("adding percentages can overflow the invariant") {
  CHECK((Percentage{30} + Percentage{20}).value() == 50);
  CHECK_THROWS_AS((void)(Percentage{60} + Percentage{60}), std::out_of_range);
}

TEST_CASE("a full ring buffer overwrites rather than failing") {
  RingBuffer buffer{3};
  buffer.push(1);
  buffer.push(2);
  buffer.push(3);
  CHECK(buffer.full());
  CHECK(buffer.front() == 1);

  // This is the whole point of a ring buffer.
  buffer.push(4);
  CHECK(buffer.size() == 3);
  CHECK(buffer.front() == 2);

  buffer.pop();
  CHECK(buffer.front() == 3);
}

TEST_CASE("the buffer stays consistent as it wraps") {
  RingBuffer buffer{2};
  for (int i = 0; i < 10; ++i) {
    buffer.push(i);
  }
  CHECK(buffer.size() == 2);
  CHECK(buffer.front() == 8);
  CHECK(buffer.capacity() == 2);
}
