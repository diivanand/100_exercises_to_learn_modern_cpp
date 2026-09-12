// Solution -- 04.07 Operator overloading
#include <doctest/doctest.h>

#include <cstdint>
#include <stdexcept>
#include <string>

class Money {
public:
  constexpr Money() = default;
  constexpr explicit Money(std::int64_t cents) : cents_(cents) {}

  constexpr std::int64_t cents() const noexcept {
    return cents_;
  }

  constexpr Money& operator+=(const Money& other) noexcept {
    cents_ += other.cents_;
    return *this;
  }

  constexpr Money& operator-=(const Money& other) noexcept {
    cents_ -= other.cents_;
    return *this;
  }

  constexpr Money& operator*=(std::int64_t factor) noexcept {
    cents_ *= factor;
    return *this;
  }

  // Hidden friends: free functions (so the left operand can convert), written
  // inside the class (so they are found by argument-dependent lookup and stay
  // next to the type they belong to).
  //
  // `lhs` is taken by value -- it is the copy we are about to return, so the
  // copy is not waste.
  friend constexpr Money operator+(Money lhs, const Money& rhs) noexcept {
    lhs += rhs;
    return lhs;
  }

  friend constexpr Money operator-(Money lhs, const Money& rhs) noexcept {
    lhs -= rhs;
    return lhs;
  }

  friend constexpr Money operator*(Money lhs, std::int64_t factor) noexcept {
    lhs *= factor;
    return lhs;
  }

  friend constexpr Money operator*(std::int64_t factor, Money rhs) noexcept {
    rhs *= factor;
    return rhs;
  }

  friend constexpr Money operator-(Money value) noexcept {
    return Money{-value.cents_};
  }

  // Defaulted equality also gives us != for free (C++20 rewrites it).
  friend constexpr bool operator==(const Money&, const Money&) = default;

private:
  std::int64_t cents_ = 0;
};

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

  CHECK(a.cents() == 100);
  CHECK(b.cents() == 25);
}

TEST_CASE("multiplication works from either side") {
  constexpr Money price{250};
  CHECK((price * 3).cents() == 750);
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
