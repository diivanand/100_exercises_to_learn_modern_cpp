// Solution -- 01.02 Braces, parentheses, and narrowing
#include <doctest/doctest.h>

#include <cstddef>
#include <vector>

std::vector<int> make_row(std::size_t width, int fill) {
  // Parentheses, not braces: "call the (count, value) constructor", not "here
  // is a list".
  //
  // clang-tidy's modernize-return-braced-init-list check suggests
  // `return {width, fill};` here. It is wrong, for precisely the reason this
  // exercise exists -- and the compiler catches it, because size_t narrows to
  // int. A static analyser's advice is advice.
  // NOLINTNEXTLINE(modernize-return-braced-init-list)
  return std::vector<int>(width, fill);
}

struct Rectangle {
  int width = 0;
  int height = 0;

  [[nodiscard]] int area() const {
    return width * height;
  }
};

TEST_CASE("make_row builds width copies of fill") {
  CHECK(make_row(3, 7) == std::vector<int>{7, 7, 7});
  CHECK(make_row(0, 7).empty());
  CHECK(make_row(1, 0) == std::vector<int>{0});
}

TEST_CASE("Rectangle is an aggregate") {
  const Rectangle r{3, 4};
  CHECK(r.area() == 12);

  const Rectangle tall{.height = 5};
  CHECK(tall.width == 0);
  CHECK(tall.area() == 0);
}

TEST_CASE("braces reject narrowing") {
  const Rectangle ok{3, 4};
  CHECK(ok.width == 3);
}
