// =============================================================================
//  04.07 -- Operator overloading
// =============================================================================
//
//  Operators are functions with punctuation for names. Overload them when your
//  type really is the thing the operator means -- a number, a vector, a
//  container, an iterator -- and not to be clever. `+` must not launch a
//  rocket (Core Guidelines C.160-C.161).
//
//  The mechanics that are easy to get wrong:
//
//   * MEMBER vs FREE. If the left operand must be convertible -- `2 * money`
//     as well as `money * 2` -- the operator has to be a free function. As a
//     member, the left operand is `*this` and never converts.
//
//   * COMPOUND FIRST. Write `operator+=` as a member (it modifies the left
//     operand), then define `operator+` as a free function in terms of it.
//     One implementation, and the two can never disagree.
//
//        friend Money operator+(Money lhs, const Money& rhs) {
//          lhs += rhs;              // lhs is a by-value copy: free to modify
//          return lhs;
//        }
//
//   * `friend` INSIDE THE CLASS is the idiomatic place for these. The function
//     is a free function found by argument-dependent lookup, but it is written
//     where the type is, and has access to the members.
//
//   * RETURN TYPES. `a += b` returns `T&`. `a + b` returns `T`. Prefix `++`
//     returns `T&`; postfix `++` takes a dummy `int` and returns the old value
//     by copy, which is why you should prefer prefix.
//
//  TASK
//    Give `Money` the arithmetic it needs. Every test below is a rule about
//    operators, not about money.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 04_07
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstdint>
#include <stdexcept>
#include <string>

// An amount in whole cents. Integer arithmetic, so no floating-point surprises.
class Money {
public:
  constexpr Money() = default;
  constexpr explicit Money(std::int64_t cents) : cents_(cents) {}

  constexpr std::int64_t cents() const noexcept {
    return cents_;
  }

  // TODO: implement the compound operators as members:
  //   Money& operator+=(const Money& other)
  //   Money& operator-=(const Money& other)
  //   Money& operator*=(std::int64_t factor)

  // TODO: implement the binary operators as hidden friends, in terms of the
  // compound ones. Include `operator*` in BOTH orders so that `2 * price`
  // works as well as `price * 2`.

  // TODO: implement unary minus, and `operator==` (defaulted is fine).

private:
  std::int64_t cents_ = 0;
};

// A user-defined literal, so amounts read as amounts. The `_` prefix is
// required for user code; the standard reserves the rest.
constexpr Money operator""_cents(unsigned long long value) {
  return Money{static_cast<std::int64_t>(value)};
}

TEST_CASE("compound assignment modifies in place and chains") {
  Money total{100};
  total += Money{50};
  CHECK(total.cents() == 150);

  total -= Money{20};
  CHECK(total.cents() == 130);

  total *= 2;
  CHECK(total.cents() == 260);

  // Returning a reference is what makes this legal.
  Money chained{0};
  (chained += Money{5}) += Money{5};
  CHECK(chained.cents() == 10);
}

TEST_CASE("binary operators produce new values") {
  constexpr Money a{100};
  constexpr Money b{25};

  CHECK((a + b).cents() == 125);
  CHECK((a - b).cents() == 75);
  CHECK((-a).cents() == -100);

  // The originals are untouched.
  CHECK(a.cents() == 100);
  CHECK(b.cents() == 25);
}

TEST_CASE("multiplication works from either side") {
  constexpr Money price{250};
  CHECK((price * 3).cents() == 750);
  // This one is only possible because operator* is a free function.
  CHECK((3 * price).cents() == 750);
}

TEST_CASE("equality and literals") {
  CHECK(Money{500} == 500_cents);
  CHECK(Money{100} + Money{50} == 150_cents);
  CHECK_FALSE(Money{1} == Money{2});
  CHECK(Money{1} != Money{2});
}

TEST_CASE("the operators are constexpr") {
  static_assert((Money{2} + Money{3}).cents() == 5);
  static_assert((100_cents - 40_cents).cents() == 60);
  CHECK(true);
}
