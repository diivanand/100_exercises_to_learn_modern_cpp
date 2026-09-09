// =============================================================================
//  01.02 -- Braces, parentheses, and narrowing
// =============================================================================
//
//  C++11 introduced brace initialisation to make one syntax work everywhere:
//
//      int         a{42};
//      Point       p{1, 2};
//      std::vector v{1, 2, 3};
//
//  It buys you two real things:
//
//   1. NO NARROWING. `int x{3.5};` is a compile error, while `int x = 3.5;`
//      silently truncates. Core Guidelines ES.23 -- "Prefer the {} initializer
//      syntax" -- is mostly about this.
//
//   2. NO MOST VEXING PARSE. `Widget w();` declares a *function*; `Widget w{};`
//      declares an object.
//
//  But there is a trap, and it is the reason ES.23 has exceptions. If a type
//  has an `std::initializer_list` constructor, braces will pick it *over*
//  every other constructor:
//
//      std::vector<int> a(3, 0);   // three elements, each 0   -> [0, 0, 0]
//      std::vector<int> b{3, 0};   // an initializer_list      -> [3, 0]
//
//  Rule of thumb: braces by default; parentheses when you mean "call this
//  constructor with these arguments" on a container-like type.
//
//  TASK
//    Fix `make_row` so it builds a row of `width` copies of `fill`, and
//    complete `Rectangle` so `area()` works. Do not change the tests.
//
//  RUN IT
//    ./mcpp test 01_02
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <vector>

std::vector<int> make_row(std::size_t width, int fill) {
  // TODO: braces here select the initializer_list constructor, so this builds
  // a two-element vector no matter what `width` says.
  return std::vector<int>{static_cast<int>(width), fill};
}

// An aggregate: no user-declared constructors, no private data, no virtuals.
// Aggregates are initialised member-by-member from a braced list, which is why
// `Rectangle r{3, 4}` works without you writing a constructor at all.
struct Rectangle {
  int width = 0;
  int height = 0;

  // TODO: return the actual area. While you are here, add `[[nodiscard]]`:
  // a caller who ignores the result has written a statement with no effect,
  // and the compiler can say so. (`const` is already right -- computing an
  // area does not modify the rectangle.)
  [[nodiscard]] int area() const {
    return width;
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

  // Members you leave out are value-initialised, not left as garbage.
  const Rectangle tall{.height = 5};
  CHECK(tall.width == 0);
  CHECK(tall.area() == 0);
}

TEST_CASE("braces reject narrowing") {
  // This is the payoff. Uncomment the next line and the build fails with
  // "type 'double' cannot be narrowed to 'int'" -- a bug caught at compile
  // time that `int bad = 3.9;` would have let through.
  //
  //   const Rectangle bad{3.9, 4.0};
  //
  // Meanwhile a value that fits exactly is fine:
  const Rectangle ok{3, 4};
  CHECK(ok.width == 3);
}
