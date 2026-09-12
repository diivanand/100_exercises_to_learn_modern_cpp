// Solution -- 03.05 The rule of five
#include <doctest/doctest.h>

#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

namespace fake_os {

inline int next_handle = 1;
inline std::vector<int> open_handles;
inline int close_count = 0;
inline int double_close_count = 0;

inline constexpr int kInvalidHandle = -1;

inline int open() {
  const int handle = next_handle++;
  open_handles.push_back(handle);
  return handle;
}

inline void close(int handle) {
  ++close_count;
  const auto it = std::ranges::find(open_handles, handle);
  if (it == open_handles.end()) {
    ++double_close_count;
    return;
  }
  open_handles.erase(it);
}

inline void reset() {
  next_handle = 1;
  open_handles.clear();
  close_count = 0;
  double_close_count = 0;
}

} // namespace fake_os

class FileHandle {
public:
  FileHandle() = default;
  explicit FileHandle(int handle) : handle_(handle) {}

  ~FileHandle() {
    if (valid()) {
      fake_os::close(handle_);
    }
  }

  // A handle has exactly one owner. Saying so with `= delete` turns a
  // double-close bug into a compile error.
  FileHandle(const FileHandle&) = delete;
  FileHandle& operator=(const FileHandle&) = delete;

  FileHandle(FileHandle&& other) noexcept
      : handle_(std::exchange(other.handle_, fake_os::kInvalidHandle)) {}

  FileHandle& operator=(FileHandle&& other) noexcept {
    if (this != &other) {
      if (valid()) {
        fake_os::close(handle_);
      }
      handle_ = std::exchange(other.handle_, fake_os::kInvalidHandle);
    }
    return *this;
  }

  int get() const noexcept {
    return handle_;
  }
  bool valid() const noexcept {
    return handle_ != fake_os::kInvalidHandle;
  }

  [[nodiscard]] int release() noexcept {
    return std::exchange(handle_, fake_os::kInvalidHandle);
  }

private:
  int handle_ = fake_os::kInvalidHandle;
};

TEST_CASE("the handle is closed exactly once") {
  fake_os::reset();
  {
    const FileHandle file{fake_os::open()};
    CHECK(file.valid());
    CHECK(fake_os::open_handles.size() == 1);
  }
  CHECK(fake_os::open_handles.empty());
  CHECK(fake_os::close_count == 1);
  CHECK(fake_os::double_close_count == 0);
}

TEST_CASE("copying is not allowed") {
  static_assert(!std::is_copy_constructible_v<FileHandle>);
  static_assert(!std::is_copy_assignable_v<FileHandle>);
  CHECK(true);
}

TEST_CASE("moving transfers ownership") {
  fake_os::reset();
  {
    FileHandle source{fake_os::open()};
    const int handle = source.get();

    FileHandle target{std::move(source)};
    CHECK(target.get() == handle);
    CHECK_FALSE(source.valid());
  }
  CHECK(fake_os::close_count == 1);
  CHECK(fake_os::double_close_count == 0);
}

TEST_CASE("move assignment closes what it was holding") {
  fake_os::reset();
  {
    FileHandle keep{fake_os::open()};
    FileHandle replace{fake_os::open()};
    CHECK(fake_os::open_handles.size() == 2);

    replace = std::move(keep);
    CHECK(fake_os::close_count == 1);
    CHECK(fake_os::open_handles.size() == 1);
  }
  CHECK(fake_os::close_count == 2);
  CHECK(fake_os::double_close_count == 0);
}

TEST_CASE("self-move does not close the handle") {
  fake_os::reset();
  {
    FileHandle file{fake_os::open()};
    FileHandle& alias = file;
    file = std::move(alias);
    CHECK(file.valid());
    CHECK(fake_os::close_count == 0);
  }
  CHECK(fake_os::close_count == 1);
}

TEST_CASE("moves are noexcept, so a vector of handles is efficient") {
  static_assert(std::is_nothrow_move_constructible_v<FileHandle>);
  static_assert(std::is_nothrow_move_assignable_v<FileHandle>);

  fake_os::reset();
  {
    std::vector<FileHandle> files;
    files.reserve(4);
    for (int i = 0; i < 4; ++i) {
      files.emplace_back(fake_os::open());
    }
    CHECK(fake_os::open_handles.size() == 4);
  }
  CHECK(fake_os::open_handles.empty());
  CHECK(fake_os::double_close_count == 0);
}
