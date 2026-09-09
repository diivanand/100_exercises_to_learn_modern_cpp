// Solution -- 03.03 Writing move operations
#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <type_traits>
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

  // Moving a std::vector steals its pointers: no allocation, nothing to throw.
  TextBuffer(TextBuffer&& other) noexcept : lines_(std::move(other.lines_)) {
    // std::vector's own move constructor already leaves `other.lines_` empty,
    // but saying so explicitly documents the postcondition our tests rely on.
    other.lines_.clear();
    ++move_count_;
  }

  TextBuffer& operator=(TextBuffer&& other) noexcept {
    if (this != &other) {
      lines_ = std::move(other.lines_);
      other.lines_.clear();
      ++move_count_;
    }
    return *this;
  }

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
  static inline int copy_count_ = 0;
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
  buffers.push_back(TextBuffer{{"b"}});

  CHECK(TextBuffer::copy_count() == 0);
  CHECK(buffers.size() == 2);
}
