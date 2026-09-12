// =============================================================================
//  08.08 -- Designing your own concepts
// =============================================================================
//
//  A concept is an INTERFACE that a type satisfies structurally -- no
//  inheritance, no registration, no coupling. That makes it the C++20 answer
//  to "what does this template actually require?", and a genuinely different
//  design tool from an abstract base class:
//
//      abstract base class     runtime dispatch, one implementation,
//                              types must derive from it
//      concept                 compile-time dispatch, one instantiation per
//                              type, types need only have the right shape
//
//  Design guidance that is easy to state and hard to follow:
//
//   * NAME THE SEMANTICS, NOT THE SYNTAX. `Sortable` is better than
//     `HasLessThan`. A concept should mean something.
//   * KEEP THEM BROAD. A concept that exactly one type satisfies is a type,
//     spelled expensively.
//   * ORDER MATTERS. A more constrained overload is preferred, so build
//     concepts by REFINEMENT: `Container` refines `Range`, and an overload
//     taking Container beats one taking Range.
//   * DO NOT OVER-CONSTRAIN. Requiring `operator<` when you only compare for
//     equality shuts out types that would have worked.
//
//  TASK
//    Design a small hierarchy of concepts for a serialisation library, and
//    watch overload resolution pick the most specific one.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 08_08
//
// =============================================================================

#include <doctest/doctest.h>

#include <concepts>
#include <cstddef>
#include <ranges>
#include <string>
#include <vector>

// TODO: a type that knows how to serialise itself: it has
// `to_string() const` returning something convertible to std::string.
template <typename T>
concept SelfDescribing = false;

// TODO: a type serialisable because it is a number.
template <typename T>
concept Numeric = false;

// TODO: a range whose ELEMENTS are serialisable. This is where it gets
// interesting: the concept is recursive, so a vector<vector<int>> satisfies it
// because vector<int> does.
//
// You will need to declare `Serialisable` before defining `SerialisableRange`,
// which C++ does not allow for concepts -- so define SerialisableRange in
// terms of the three cases directly, or restructure. Think about it before
// reaching for the solution.
template <typename T>
concept SerialisableRange = false;

// TODO: the union of the three.
template <typename T>
concept Serialisable = false;

// TODO: three overloads of `serialise`, one per concept, most specific first.
//
//   * Numeric        -> std::to_string(value)
//   * SelfDescribing -> value.to_string()
//   * a range        -> "[a,b,c]" with the elements serialised recursively
//
// Overload resolution will pick the most constrained one that applies, so you
// do not need to exclude the others by hand.
template <typename T>
std::string serialise(const T&) {
  return "?";
}

struct Point {
  int x = 0;
  int y = 0;

  std::string to_string() const {
    return "(" + std::to_string(x) + "," + std::to_string(y) + ")";
  }
};

struct Opaque {
  int hidden = 0;
};

TEST_CASE("the concepts classify types correctly") {
  static_assert(Numeric<int>);
  static_assert(Numeric<double>);
  static_assert(!Numeric<std::string>);

  static_assert(SelfDescribing<Point>);
  static_assert(!SelfDescribing<int>);
  static_assert(!SelfDescribing<Opaque>);

  static_assert(SerialisableRange<std::vector<int>>);
  static_assert(SerialisableRange<std::vector<Point>>);
  static_assert(!SerialisableRange<std::vector<Opaque>>);

  static_assert(Serialisable<int>);
  static_assert(Serialisable<Point>);
  static_assert(Serialisable<std::vector<Point>>);
  static_assert(!Serialisable<Opaque>);
  CHECK(true);
}

TEST_CASE("numbers") {
  CHECK(serialise(42) == "42");
  CHECK(serialise(-1) == "-1");
}

TEST_CASE("self-describing types") {
  CHECK(serialise(Point{1, 2}) == "(1,2)");
}

TEST_CASE("ranges, recursively") {
  CHECK(serialise(std::vector<int>{1, 2, 3}) == "[1,2,3]");
  CHECK(serialise(std::vector<int>{}) == "[]");
  CHECK(serialise(std::vector<Point>{{1, 2}, {3, 4}}) == "[(1,2),(3,4)]");

  const std::vector<std::vector<int>> nested = {{1, 2}, {3}};
  CHECK(serialise(nested) == "[[1,2],[3]]");
}

TEST_CASE("an unserialisable type is rejected at the call") {
  // Uncomment and the error names the constraint that failed, rather than
  // failing somewhere inside the function body:
  //
  //   serialise(Opaque{});
  CHECK(true);
}
