// =============================================================================
//  03.05 -- The rule of five, and when you actually need it
// =============================================================================
//
//  The rule of zero has one exception: the class whose *whole job* is to own a
//  resource that the standard library does not already wrap. A file handle, a
//  socket, a C library's opaque pointer. Somebody has to write that class, and
//  when you do, you write all five special members:
//
//      ~T();                         destructor
//      T(const T&);                  copy constructor
//      T& operator=(const T&);       copy assignment
//      T(T&&) noexcept;              move constructor
//      T& operator=(T&&) noexcept;   move assignment
//
//  Declare one and you must consider all five -- that is the rule. "Consider"
//  often means `= delete`: a unique handle that cannot be duplicated should
//  say so rather than pretend.
//
//  Core Guidelines C.21: "If you define or =delete any copy, move, or
//  destructor function, define or =delete them all".
//
//  TASK
//    `FileHandle` wraps a fake OS handle. Make it a correct, move-only owner:
//    non-copyable, movable, and it must close its handle exactly once.
//
//  NOTE  This exercise starts as a compile error; the tests use operations
//        that do not exist yet.
//
//  RUN IT
//    ./mcpp test 03_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

// A stand-in for an operating system API. `open` hands out a handle, `close`
// gives it back, and closing the same handle twice is an error.
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

  // TODO: implement the rule of five.
  //
  //  * The destructor closes the handle -- but only if it owns one.
  //  * Copying must be `= delete`d: two owners would close the same handle.
  //  * The move constructor takes the handle and leaves `other` invalid.
  //    `std::exchange` is exactly the tool: it assigns a new value and returns
  //    the old one, in a single expression usable in a member initialiser.
  //  * Move assignment must close whatever it is already holding first, and
  //    must survive `x = std::move(x)`.

  [[nodiscard]] int get() const noexcept {
    return handle_;
  }
  [[nodiscard]] bool valid() const noexcept {
    return handle_ != fake_os::kInvalidHandle;
  }

  // Gives up ownership without closing -- the escape hatch for handing the
  // handle to a C API that will close it for you.
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
    // The handle `replace` used to own has been closed already.
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
