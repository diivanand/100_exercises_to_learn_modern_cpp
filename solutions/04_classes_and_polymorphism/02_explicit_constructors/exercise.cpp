// Solution -- 04.02 explicit
#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <type_traits>
#include <utility>

class Timeout {
public:
  explicit Timeout(int seconds) : seconds_(seconds) {}
  [[nodiscard]] int seconds() const noexcept {
    return seconds_;
  }

private:
  int seconds_;
};

struct Point {
  int x = 0;
  int y = 0;
};

class Name {
public:
  explicit Name(std::string value) : value_(std::move(value)) {}
  [[nodiscard]] const std::string& value() const noexcept {
    return value_;
  }

private:
  std::string value_;
};

template <typename T>
class Wrapper {
public:
  // C++20: the constructor is explicit exactly when the wrapped conversion is
  // not implicit. This is how std::pair, std::optional and std::tuple decide.
  //
  // The constraint matters: a forwarding constructor without it is a better
  // match than the copy constructor for `Wrapper<long> b{a};` (U deduces to
  // `Wrapper<long>&`, an exact match), and then fails trying to build a `long`
  // from a Wrapper. Excluding our own type hands copies back to the copy
  // constructor.
  template <typename U>
    requires(!std::is_same_v<std::remove_cvref_t<U>, Wrapper>)
  explicit(!std::is_convertible_v<U, T>) Wrapper(U&& value)
      : value_(std::forward<U>(value)) {}

  [[nodiscard]] const T& get() const noexcept {
    return value_;
  }

private:
  T value_;
};

int wait(Timeout timeout) {
  return timeout.seconds();
}

TEST_CASE("a Timeout must be spelled out") {
  static_assert(!std::is_convertible_v<int, Timeout>);
  static_assert(std::is_constructible_v<Timeout, int>);

  CHECK(wait(Timeout{30}) == 30);
}

TEST_CASE("a Point still reads well as a braced pair") {
  const Point p = {1, 2};
  CHECK(p.x == 1);
  CHECK(p.y == 2);
}

TEST_CASE("a Name must be spelled out") {
  static_assert(!std::is_convertible_v<std::string, Name>);
  static_assert(!std::is_convertible_v<const char*, Name>);
  const Name name{"ada"};
  CHECK(name.value() == "ada");
}

TEST_CASE("Wrapper follows the conversion it wraps") {
  static_assert(std::is_convertible_v<int, Wrapper<long>>);

  static_assert(!std::is_convertible_v<std::string, Wrapper<Name>>);
  static_assert(std::is_constructible_v<Wrapper<Name>, std::string>);

  const Wrapper<long> wrapped = 42;
  CHECK(wrapped.get() == 42);
}
