// =============================================================================
//  05.05 -- std::variant
// =============================================================================
//
//  A `std::variant<A, B, C>` holds exactly one of its alternatives, and knows
//  which. It is a type-safe union: no manual tag, no way to read the wrong
//  member, and destructors are run correctly when the alternative changes.
//
//  Where it earns its place: a value that is genuinely one of a CLOSED set of
//  shapes. A JSON value. A parsed token. A state machine's current state. A
//  result that is either a value or an error (05.07).
//
//  Closed is the operative word. Adding an alternative means recompiling every
//  place that inspects one -- which is a feature when you want the compiler to
//  find them all, and a reason to prefer inheritance or type erasure (04.09)
//  when the set is open.
//
//  The interface:
//
//      v.index()                     which alternative, as a number
//      std::holds_alternative<T>(v)  is it a T?
//      std::get<T>(v)                the T, or throws std::bad_variant_access
//      std::get_if<T>(&v)            a T*, or nullptr -- no exception
//      std::visit(f, v)              call f with whatever is inside (05.06)
//
//  A default-constructed variant holds its FIRST alternative, so put something
//  harmless first -- often `std::monostate`, an empty type that exists exactly
//  for this.
//
//  TASK
//    Model a small JSON-ish value with a variant and implement the three
//    functions below.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 05_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <variant>
#include <vector>

// TODO: define `Value` as a variant that can hold, in this order:
//   std::monostate (null), bool, double, std::string
//
// Putting monostate first means a default-constructed Value is null, which is
// the only sensible default.
using Value = std::monostate;

// TODO: return "null", "bool", "number" or "string" depending on what is
// stored. Use `std::holds_alternative` or `index()`; you will do the same job
// far more neatly with std::visit in the next exercise.
std::string type_name(const Value& value) {
  return "?";
}

// TODO: return the number if the value holds one, and 0.0 otherwise. Use
// `std::get_if`, which returns a pointer rather than throwing -- the right
// tool when "not a number" is an expected case rather than an error.
double as_number(const Value& value) {
  return 0.0;
}

// TODO: return true for JSON's falsy values: null, false, 0, and the empty
// string. Everything else is truthy.
bool is_falsy(const Value& value) {
  return false;
}

TEST_CASE("a default-constructed Value is null") {
  const Value value;
  CHECK(type_name(value) == "null");
  CHECK(value.index() == 0);
  CHECK(std::holds_alternative<std::monostate>(value));
}

TEST_CASE("each alternative is recognised") {
  CHECK(type_name(Value{true}) == "bool");
  CHECK(type_name(Value{3.5}) == "number");
  CHECK(type_name(Value{std::string{"text"}}) == "string");
}

TEST_CASE("assignment changes the alternative") {
  Value value = 1.5;
  CHECK(type_name(value) == "number");

  value = std::string{"now a string"};
  CHECK(type_name(value) == "string");
  CHECK(std::get<std::string>(value) == "now a string");

  // Reading the wrong alternative throws rather than reinterpreting memory --
  // the whole reason to prefer this over a union.
  CHECK_THROWS_AS((void)std::get<double>(value), std::bad_variant_access);
}

TEST_CASE("as_number does not throw for other types") {
  CHECK(as_number(Value{42.0}) == doctest::Approx(42.0));
  CHECK(as_number(Value{std::string{"x"}}) == doctest::Approx(0.0));
  CHECK(as_number(Value{}) == doctest::Approx(0.0));
}

TEST_CASE("falsiness") {
  CHECK(is_falsy(Value{}));
  CHECK(is_falsy(Value{false}));
  CHECK(is_falsy(Value{0.0}));
  CHECK(is_falsy(Value{std::string{}}));

  CHECK_FALSE(is_falsy(Value{true}));
  CHECK_FALSE(is_falsy(Value{0.5}));
  CHECK_FALSE(is_falsy(Value{std::string{"x"}}));
}

TEST_CASE("a variant is as big as its largest alternative, plus a tag") {
  static_assert(sizeof(Value) >= sizeof(std::string));
  CHECK(true);
}
