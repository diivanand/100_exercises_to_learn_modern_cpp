// =============================================================================
//  04.06 -- = default and = delete
// =============================================================================
//
//  `= default` asks for the compiler's version explicitly. It is not the same
//  as writing the body yourself:
//
//      Foo() {}            // user-PROVIDED: Foo is not trivial, and
//                          // `Foo f{};` leaves members uninitialised
//      Foo() = default;    // user-DECLARED: still trivial, still an aggregate
//                          // candidate, and value-initialisation zeroes it
//
//  That difference decides whether a type is trivially copyable, whether it
//  can live in a `constexpr` variable, and whether memcpy-based code is legal.
//
//  `= delete` removes a function. Two uses:
//
//   1. Say "this operation does not apply": a non-copyable handle deletes its
//      copy constructor (03.05). The error message is then about intent, not
//      about a missing private declaration.
//
//   2. Poison an unwanted CONVERSION. Deleted functions still take part in
//      overload resolution, so a deleted overload is chosen and then rejected:
//
//          void print(int);
//          void print(char) = delete;    // print('x') is now an error
//
//  Deleted functions must be declared before use, and are best kept public --
//  a private deleted function produces a worse diagnostic.
//
//  TASK
//    Make `Vec2` trivially copyable and usable in a constant expression, make
//    `Handle` move-only, and stop `schedule` from silently accepting a double.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 04_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <type_traits>
#include <utility>

// TODO: the user-PROVIDED default constructor makes Vec2 non-trivial, so it
// cannot be memcpy-ed and `constexpr Vec2 origin{};` does not zero it.
// Replace `Vec2() {}` with `Vec2() = default;`, and make the two-argument
// constructor constexpr.
//
// Resist the urge to also write `double x = 0.0;`. A default member
// initialiser makes the default constructor non-trivial again -- it now has
// work to do. Leaving the members bare and letting `Vec2{}` value-initialise
// them keeps the type trivial *and* still gives you zeroes. This is the one
// place where "always initialise your members" and "keep the type trivial"
// genuinely pull in opposite directions.
struct Vec2 {
  Vec2() {}
  Vec2(double x, double y) : x(x), y(y) {}

  double x;
  double y;
};

// TODO: a Handle owns a slot and must not be copied. Delete the copy
// operations and default the move ones.
class Handle {
public:
  Handle() = default;
  explicit Handle(int slot) : slot_(slot) {}

  [[nodiscard]] int slot() const noexcept {
    return slot_;
  }

private:
  int slot_ = -1;
};

// Schedules work after `milliseconds`. Passing 0.5 should be an error, not a
// silent truncation to 0.
// TODO: add a deleted overload that catches floating-point arguments.
std::string schedule(int milliseconds) {
  return "in " + std::to_string(milliseconds) + "ms";
}

TEST_CASE("Vec2 is a trivial, constexpr-friendly value type") {
  static_assert(std::is_trivially_copyable_v<Vec2>);
  static_assert(std::is_trivially_default_constructible_v<Vec2>);

  // Value-initialisation zeroes a trivially-default-constructible type.
  constexpr Vec2 origin{};
  static_assert(origin.x == 0.0);
  static_assert(origin.y == 0.0);

  constexpr Vec2 point{3.0, 4.0};
  static_assert(point.x == 3.0);
  CHECK(point.y == doctest::Approx(4.0));
}

TEST_CASE("Handle is move-only") {
  static_assert(!std::is_copy_constructible_v<Handle>);
  static_assert(!std::is_copy_assignable_v<Handle>);
  static_assert(std::is_nothrow_move_constructible_v<Handle>);
  static_assert(std::is_nothrow_move_assignable_v<Handle>);

  Handle source{7};
  const Handle target{std::move(source)};
  CHECK(target.slot() == 7);
}

TEST_CASE("schedule refuses a floating-point argument") {
  CHECK(schedule(250) == "in 250ms");

  // A deleted overload makes this a compile error rather than "in 0ms":
  //
  //   schedule(0.5);
  //
  // Deleted functions participate in overload resolution -- that is exactly
  // why this works, and why simply not declaring the overload would not.
}
