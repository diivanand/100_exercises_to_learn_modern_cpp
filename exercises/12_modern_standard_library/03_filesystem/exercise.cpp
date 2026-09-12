// =============================================================================
//  12.03 -- std::filesystem
// =============================================================================
//
//  `<filesystem>` gives paths a type, and file operations a portable,
//  exception-aware interface. Before it, every project had its own string
//  manipulation for "join these two path components" -- and every one of them
//  was wrong on some platform.
//
//  `std::filesystem::path` is not a string. It knows about separators, and its
//  `operator/` joins components properly:
//
//      path base = "/var/log";
//      path file = base / "app" / "current.log";
//
//      file.filename()        "current.log"
//      file.stem()            "current"
//      file.extension()       ".log"
//      file.parent_path()     "/var/log/app"
//      file.is_absolute()
//
//  QUERIES: `exists`, `is_directory`, `is_regular_file`, `file_size`,
//  `last_write_time`.
//
//  ITERATION: `directory_iterator` for one level,
//  `recursive_directory_iterator` for the whole tree. Both are ranges, so
//  chapter 7's views work on them.
//
//  ERRORS, TWICE OVER. Every operation has two overloads: one that throws
//  `filesystem_error`, and one taking a `std::error_code&` that does not. Use
//  the throwing one when a failure is exceptional, the error_code one when it
//  is expected -- 05.01's judgement, made concrete by the library.
//
//  RACES ARE UNAVOIDABLE: `if (exists(p)) { read(p); }` can be wrong by the
//  time the second line runs. Prefer "try, and handle the failure".
//
//  TASK
//    Implement the five functions.
//
//  NOTE  This exercise starts as a compile error. It also touches the real
//        filesystem -- inside a temporary directory it creates and removes.
//
//  RUN IT
//    ./mcpp test 12_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

// TODO: join a directory and a filename. Do not concatenate strings -- use
// operator/, which knows what a separator is.
fs::path join(const fs::path& directory, const std::string& filename) {
  return {};
}

// TODO: return the extension WITHOUT the leading dot, or an empty string if
// there is none. `path::extension()` includes the dot.
std::string extension_of(const fs::path& file) {
  return {};
}

// TODO: return the names of the regular files directly inside `directory`,
// sorted. Skip subdirectories. Use directory_iterator.
std::vector<std::string> file_names(const fs::path& directory) {
  return {};
}

// TODO: return the total size in bytes of every regular file in the tree.
// recursive_directory_iterator, and file_size.
std::uintmax_t total_size(const fs::path& directory) {
  return 0;
}

// Returns the size of a file, or 0 if it does not exist or cannot be read.
//
// TODO: use the std::error_code overload of file_size. A missing file is an
// expected outcome here, not an exceptional one -- so it should not cost an
// exception.
std::uintmax_t size_or_zero(const fs::path& file) {
  return 0;
}

namespace {

// Creates a small directory tree in a unique temporary directory, and removes
// it again -- RAII (03.06) applied to something outside the program.
class TemporaryTree {
public:
  TemporaryTree() {
    root_ = fs::temp_directory_path() / ("mcpp-12-03-" + std::to_string(::getpid()));
    fs::remove_all(root_);
    fs::create_directories(root_ / "nested");

    write(root_ / "alpha.txt", "12345");
    write(root_ / "beta.log", "1234567890");
    write(root_ / "no_extension", "1");
    write(root_ / "nested" / "gamma.txt", "1234");
  }

  ~TemporaryTree() {
    std::error_code ignored;
    fs::remove_all(root_, ignored);
  }

  TemporaryTree(const TemporaryTree&) = delete;
  TemporaryTree& operator=(const TemporaryTree&) = delete;

  const fs::path& root() const {
    return root_;
  }

private:
  static void write(const fs::path& file, const std::string& contents) {
    std::ofstream out{file, std::ios::binary};
    out << contents;
  }

  fs::path root_;
};

} // namespace

TEST_CASE("paths join without string surgery") {
  const fs::path joined = join("/var/log", "app.log");
  CHECK(joined.filename() == "app.log");
  CHECK(joined.parent_path() == "/var/log");

  // A trailing separator on the directory must not produce a doubled one.
  CHECK(join("/var/log/", "app.log") == fs::path{"/var/log/app.log"});
}

TEST_CASE("path components") {
  CHECK(extension_of("report.pdf") == "pdf");
  CHECK(extension_of("archive.tar.gz") == "gz");
  CHECK(extension_of("README") == "");
  CHECK(extension_of("/a/b/c.txt") == "txt");
}

TEST_CASE("listing one directory level") {
  const TemporaryTree tree;
  const auto names = file_names(tree.root());

  CHECK(names == std::vector<std::string>{"alpha.txt", "beta.log", "no_extension"});
}

TEST_CASE("walking a tree") {
  const TemporaryTree tree;
  // 5 + 10 + 1 + 4 bytes.
  CHECK(total_size(tree.root()) == 20);
}

TEST_CASE("an expected failure does not need an exception") {
  const TemporaryTree tree;
  CHECK(size_or_zero(tree.root() / "alpha.txt") == 5);
  CHECK(size_or_zero(tree.root() / "does_not_exist") == 0);

  // The throwing overload is the right one when absence really is an error.
  CHECK_THROWS_AS((void)fs::file_size(tree.root() / "does_not_exist"),
                  fs::filesystem_error);
}
