// =============================================================================
//  04.02 -- explicit
// =============================================================================
//
//  A single-argument constructor is also a CONVERSION. Without `explicit`, the
//  compiler will use it silently, anywhere a conversion is allowed:
//
//      class Timeout { public: Timeout(int seconds); };
//      void wait(Timeout t);
//
//      wait(30);        // compiles. Is 30 seconds? milliseconds? a retry count?
//      if (timeout == 30) ...   // compiles too, and means something odd
//
//  `explicit` says "you may construct one of these from an int, but only when
//  you say so". Core Guidelines C.46: "By default, declare single-argument
//  constructors explicit".
//
//  C++11 extended `explicit` to multi-argument constructors, because braced
//  initialisation made those implicit too:
//
//      Rect make() { return {1, 2, 3, 4}; }   // implicit, unless explicit
//
//  And C++20 added `explicit(bool)`, so a template can decide: `std::pair`
//  uses it to be implicit exactly when both element conversions are.
//
//  The exception is the conversion you genuinely want to be invisible, where
//  the types mean the same thing -- `std::string` from `const char*` is the
//  canonical example.
//
//  TASK
//    Add `explicit` where it belongs, and use `explicit(bool)` to make
//    `Wrapper` implicit only when the wrapped conversion itself is.
//
//  NOTE  This exercise starts by compiling too much: the tests assert that
//        certain conversions are *rejected*.
//
//  RUN IT
//    ./mcpp test 04_02
//
// =============================================================================

#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <type_traits>
#include <utility>

// A duration in seconds. Nobody should be able to pass a bare number.
// TODO: make the constructor explicit.
class Timeout {
public:
  Timeout(int seconds) : seconds_(seconds) {}
  int seconds() const noexcept {
    return seconds_;
  }

private:
  int seconds_;
};

// A point. The two-argument constructor is implicitly usable from a braced
// list, which is fine here -- `{1, 2}` reads as a point.
struct Point {
  int x = 0;
  int y = 0;
};

// A name. Converting from const char* to a Name silently is exactly the kind
// of thing that turns a typo into a valid call.
// TODO: make this explicit too.
class Name {
public:
  Name(std::string value) : value_(std::move(value)) {}
  const std::string& value() const noexcept {
    return value_;
  }

private:
  std::string value_;
};

// A generic wrapper. It should be implicitly constructible from a U exactly
// when U converts implicitly to T -- no more, no less.
//
// TODO: use `explicit(!std::is_convertible_v<U, T>)`.
template <typename T>
class Wrapper {
public:
  template <typename U>
  Wrapper(U&& value) : value_(std::forward<U>(value)) {}

  const T& get() const noexcept {
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
  // int -> long is an implicit conversion, so this wrapper is implicit too.
  static_assert(std::is_convertible_v<int, Wrapper<long>>);

  // std::string -> Name is not (you just made it explicit), so neither is the
  // wrapper.
  static_assert(!std::is_convertible_v<std::string, Wrapper<Name>>);
  static_assert(std::is_constructible_v<Wrapper<Name>, std::string>);

  const Wrapper<long> wrapped = 42;
  CHECK(wrapped.get() == 42);
}
