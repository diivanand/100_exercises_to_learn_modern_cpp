// =============================================================================
//  03.03 -- Writing move operations
// =============================================================================
//
//  A move constructor takes the resources out of its argument and leaves it in
//  a state that is safe to destroy and safe to assign to. That is the entire
//  contract. "Valid but unspecified" is not hand-waving -- it is a promise you
//  are making to whoever holds the moved-from object.
//
//  Three rules for writing them:
//
//   1. LEAVE THE SOURCE EMPTY, not merely "not owning". A moved-from object
//      whose destructor still frees the pointer you stole is a double free.
//      `std::exchange(ptr, nullptr)` does the steal-and-clear in one
//      expression, in the member initialiser list where it belongs.
//
//   2. MARK THEM noexcept (see 02.04) -- containers check.
//
//   3. MOVE ASSIGNMENT MUST RELEASE WHAT IT ALREADY HOLDS, and must survive
//      self-move. The simplest correct form is to swap and let the source's
//      destructor do the cleanup.
//
//  TASK
//    `TextBuffer` has a working copy constructor and no move operations, so
//    every "move" is a copy. Add move construction and move assignment.
//
//
//  NOTE  This exercise starts as a compile error: the last test asks the
//        compiler whether TextBuffer has noexcept move operations, and it does
//        not yet.
//
//  RUN IT
//    ./mcpp test 03_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

class TextBuffer {
public:
  TextBuffer() = default;

  explicit TextBuffer(std::vector<std::string> lines) : lines_(std::move(lines)) {}

  TextBuffer(const TextBuffer& other) : lines_(other.lines_) {
    ++copy_count_;
  }

  TextBuffer& operator=(const TextBuffer& other) {
    if (this != &other) {
      lines_ = other.lines_;
      ++copy_count_;
    }
    return *this;
  }

  // TODO: add a move constructor and a move assignment operator. Both must be
  // noexcept, both must increment `move_count_`, and both must leave `other`
  // empty.
  //
  // Note that declaring the copy operations above suppressed the compiler's
  // implicit move operations -- which is why every "move" in the tests below
  // is currently landing on the copy path.

  ~TextBuffer() = default;

  [[nodiscard]] std::size_t size() const noexcept {
    return lines_.size();
  }
  [[nodiscard]] bool empty() const noexcept {
    return lines_.empty();
  }
  [[nodiscard]] const std::string& line(std::size_t index) const {
    return lines_[index];
  }

  [[nodiscard]] static int copy_count() noexcept {
    return copy_count_;
  }
  [[nodiscard]] static int move_count() noexcept {
    return move_count_;
  }
  static void reset_counts() noexcept {
    copy_count_ = 0;
    move_count_ = 0;
  }

private:
  std::vector<std::string> lines_;
  static inline int copy_count_ = 0; // C++17 inline static: no .cpp needed
  static inline int move_count_ = 0;
};

TEST_CASE("moving does not copy") {
  TextBuffer::reset_counts();

  TextBuffer source{{"alpha", "beta"}};
  TextBuffer target{std::move(source)};

  CHECK(TextBuffer::move_count() == 1);
  CHECK(TextBuffer::copy_count() == 0);
  CHECK(target.size() == 2);
  CHECK(target.line(0) == "alpha");
  CHECK(source.empty());
}

TEST_CASE("move assignment releases what it held") {
  TextBuffer::reset_counts();

  TextBuffer source{{"one", "two", "three"}};
  TextBuffer target{{"discard me"}};
  target = std::move(source);

  CHECK(TextBuffer::move_count() == 1);
  CHECK(TextBuffer::copy_count() == 0);
  CHECK(target.size() == 3);
  CHECK(source.empty());
}

TEST_CASE("copying still copies") {
  TextBuffer::reset_counts();

  const TextBuffer source{{"shared"}};
  const TextBuffer target{source};

  CHECK(TextBuffer::copy_count() == 1);
  CHECK(TextBuffer::move_count() == 0);
  CHECK(source.size() == 1);
  CHECK(target.size() == 1);
}

TEST_CASE("the move operations are noexcept, so containers use them") {
  static_assert(std::is_nothrow_move_constructible_v<TextBuffer>);
  static_assert(std::is_nothrow_move_assignable_v<TextBuffer>);

  TextBuffer::reset_counts();
  std::vector<TextBuffer> buffers;
  buffers.reserve(1);
  buffers.emplace_back(std::vector<std::string>{"a"});
  buffers.push_back(TextBuffer{{"b"}}); // forces a reallocation

  CHECK(TextBuffer::copy_count() == 0);
  CHECK(buffers.size() == 2);
}
