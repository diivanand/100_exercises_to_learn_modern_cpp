// Solution -- 05.08 Invariants, preconditions, and where to check them
#include <doctest/doctest.h>

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

class Percentage {
public:
  // The value comes from outside the program, so a bad one is data, not a bug.
  explicit Percentage(int value) : value_(value) {
    if (value < 0 || value > 100) {
      throw std::out_of_range{"percentage must be in 0..100"};
    }
  }

  int value() const noexcept {
    return value_;
  }

  Percentage operator+(const Percentage& other) const {
    // Both operands are already in range -- that is the invariant. Only the
    // sum can escape it, and the constructor is where that is checked, so
    // there is exactly one place in the class that knows the range.
    return Percentage{value_ + other.value_};
  }

private:
  int value_;
};

class RingBuffer {
public:
  // A zero capacity is not a value a correct caller can pass: every other
  // operation would divide by zero. That makes it a programming error, so an
  // assert is right -- and it costs nothing in a release build.
  explicit RingBuffer(std::size_t capacity) : storage_(capacity) {
    assert(capacity > 0 && "a ring buffer needs room for at least one element");
  }

  // Overwriting the oldest element is the defined behaviour of a ring buffer,
  // not a failure. Nothing to check.
  void push(int value) {
    storage_[(head_ + size_) % storage_.size()] = value;
    if (size_ == storage_.size()) {
      head_ = (head_ + 1) % storage_.size();
    } else {
      ++size_;
    }
  }

  // A precondition: there is no correct answer for an empty buffer, and the
  // caller can always check size() first.
  int front() const {
    assert(size_ > 0 && "front() on an empty buffer");
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
