// Solution -- 02.04 noexcept
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

  Buffer(Buffer&& other) noexcept : data_(std::move(other.data_)) {}

  Buffer& operator=(Buffer&& other) noexcept {
    data_ = std::move(other.data_);
    return *this;
  }

  ~Buffer() = default;

  [[nodiscard]] std::size_t size() const noexcept {
    return data_.size();
  }

  void swap(Buffer& other) noexcept {
    data_.swap(other.data_);
  }

private:
  std::vector<unsigned char> data_;
};

void swap(Buffer& lhs, Buffer& rhs) noexcept {
  lhs.swap(rhs);
}

TEST_CASE("moving a Buffer is declared noexcept") {
  static_assert(std::is_nothrow_move_constructible_v<Buffer>);
  static_assert(std::is_nothrow_move_assignable_v<Buffer>);
  static_assert(noexcept(std::declval<Buffer&>().swap(std::declval<Buffer&>())));
  CHECK(true);
}

TEST_CASE("a vector of Buffers moves rather than copies when it grows") {
  static_assert(
      std::is_same_v<decltype(std::move_if_noexcept(std::declval<Buffer&>())), Buffer&&>);
  CHECK(true);
}

TEST_CASE("moving still does the right thing at run time") {
  Buffer source{16};
  Buffer target{std::move(source)};
  CHECK(target.size() == 16);
  CHECK(source.size() == 0);

  Buffer a{4};
  Buffer b{9};
  swap(a, b);
  CHECK(a.size() == 9);
  CHECK(b.size() == 4);
}
