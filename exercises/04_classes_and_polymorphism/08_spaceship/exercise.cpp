// =============================================================================
//  04.08 -- The three-way comparison operator (C++20)
// =============================================================================
//
//  Before C++20, making a type ordered meant writing six operators, five of
//  which were boilerplate that could disagree with the sixth.
//
//  `operator<=>` returns a comparison CATEGORY rather than a bool, and the
//  compiler synthesises <, >, <=, >= from it. Defaulting it compares members
//  in declaration order, which is exactly lexicographic ordering:
//
//      auto operator<=>(const Version&) const = default;
//
//  The three categories say how strong the ordering is:
//
//    strong_ordering    equivalent values are INTERCHANGEABLE. Integers.
//    weak_ordering      equivalent values may still be distinguishable --
//                       case-insensitive strings, where "ABC" and "abc" are
//                       equivalent but not the same.
//    partial_ordering   some pairs are unordered. Floating point: every
//                       comparison with NaN is `unordered`.
//
//  Two rules that catch people out:
//
//   * `<=>` does NOT generate `==`. Equality can often be computed faster
//     (compare sizes first), so the standard keeps them separate. Default them
//     both, or define both.
//
//   * Defaulting `==` gives you `!=`; defaulting `<=>` gives you the four
//     relational operators. You write two lines and get six operators.
//
//  TASK
//    Give `Version` a defaulted ordering, and give `CaseInsensitive` a
//    hand-written weak ordering.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 04_08
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <cctype>
#include <compare>
#include <string>
#include <utility>
#include <vector>

// Ordered by major, then minor, then patch -- which is exactly what comparing
// the members in declaration order does.
struct Version {
  int major = 0;
  int minor = 0;
  int patch = 0;

  // TODO: default both `operator==` and `operator<=>`. Two lines replace six
  // operators, and they cannot drift apart.
};

// A string that compares without regard to case. "ABC" and "abc" are
// equivalent but plainly not identical, which is the definition of a WEAK
// ordering.
class CaseInsensitive {
public:
  explicit CaseInsensitive(std::string value) : value_(std::move(value)) {}

  [[nodiscard]] const std::string& value() const noexcept {
    return value_;
  }

  // TODO: implement `operator<=>` returning std::weak_ordering, and
  // `operator==`, both comparing case-insensitively.
  //
  // Compare character by character after lowering the case; when one string is
  // a prefix of the other, the shorter one comes first.
  //
  // (`std::tolower` takes an int and misbehaves on negative chars, so cast
  // through `unsigned char` first -- a genuine trap, not pedantry.)

private:
  std::string value_;
};

TEST_CASE("Version gets all six operators from two defaults") {
  constexpr Version v1{1, 2, 3};
  constexpr Version v2{1, 3, 0};

  CHECK(v1 < v2);
  CHECK(v1 <= v2);
  CHECK(v2 > v1);
  CHECK(v2 >= v1);
  CHECK(v1 != v2);
  CHECK(v1 == Version{1, 2, 3});

  // The ordering is lexicographic in declaration order.
  CHECK(Version{2, 0, 0} > Version{1, 99, 99});
}

TEST_CASE("the comparison category is strong") {
  static_assert(std::is_same_v<decltype(Version{} <=> Version{}), std::strong_ordering>);
  CHECK((Version{1, 0, 0} <=> Version{1, 0, 0}) == std::strong_ordering::equal);
  CHECK(true);
}

TEST_CASE("sorting comes for free") {
  std::vector<Version> versions = {{1, 10, 0}, {1, 2, 0}, {0, 9, 9}, {1, 2, 1}};
  std::sort(versions.begin(), versions.end());
  CHECK(versions.front() == Version{0, 9, 9});
  CHECK(versions.back() == Version{1, 10, 0});
}

TEST_CASE("CaseInsensitive is weakly ordered") {
  const CaseInsensitive upper{"HELLO"};
  const CaseInsensitive lower{"hello"};

  CHECK(upper == lower);
  CHECK((upper <=> lower) == std::weak_ordering::equivalent);
  // Equivalent, but not interchangeable -- the values still differ.
  CHECK(upper.value() != lower.value());

  static_assert(std::is_same_v<decltype(std::declval<CaseInsensitive>() <=>
                                        std::declval<CaseInsensitive>()),
                               std::weak_ordering>);
}

TEST_CASE("CaseInsensitive orders like a dictionary") {
  CHECK(CaseInsensitive{"apple"} < CaseInsensitive{"Banana"});
  CHECK(CaseInsensitive{"Zebra"} > CaseInsensitive{"apple"});
  CHECK(CaseInsensitive{"pre"} < CaseInsensitive{"prefix"});
}
