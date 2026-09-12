// Solution -- 12.03 std::filesystem
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

fs::path join(const fs::path& directory, const std::string& filename) {
  return directory / filename;
}

std::string extension_of(const fs::path& file) {
  const std::string extension = file.extension().string();
  return extension.empty() ? std::string{} : extension.substr(1);
}

std::vector<std::string> file_names(const fs::path& directory) {
  std::vector<std::string> names;
  for (const auto& entry : fs::directory_iterator{directory}) {
    if (entry.is_regular_file()) {
      names.push_back(entry.path().filename().string());
    }
  }
  // directory_iterator's order is unspecified, so anything that compares the
  // result has to sort it.
  std::ranges::sort(names);
  return names;
}

std::uintmax_t total_size(const fs::path& directory) {
  std::uintmax_t total = 0;
  for (const auto& entry : fs::recursive_directory_iterator{directory}) {
    if (entry.is_regular_file()) {
      total += entry.file_size();
    }
  }
  return total;
}

std::uintmax_t size_or_zero(const fs::path& file) {
  // The error_code overload: a missing file is an expected answer here, and
  // an exception would be the wrong tool for it (05.01).
  std::error_code error;
  const std::uintmax_t size = fs::file_size(file, error);
  return error ? 0 : size;
}

namespace {

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
  CHECK(total_size(tree.root()) == 20);
}

TEST_CASE("an expected failure does not need an exception") {
  const TemporaryTree tree;
  CHECK(size_or_zero(tree.root() / "alpha.txt") == 5);
  CHECK(size_or_zero(tree.root() / "does_not_exist") == 0);

  CHECK_THROWS_AS((void)fs::file_size(tree.root() / "does_not_exist"),
                  fs::filesystem_error);
}
