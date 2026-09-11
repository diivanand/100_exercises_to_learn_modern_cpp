// =============================================================================
//  08.05 -- Non-type template parameters
// =============================================================================
//
//  A template parameter does not have to be a type. It can be a value, and
//  that value is then part of the type:
//
//      template <std::size_t N> class FixedBuffer;
//      FixedBuffer<16> a;
//      FixedBuffer<32> b;    // a DIFFERENT type; they cannot be mixed
//
//  `std::array<T, N>` is the standard's example. So is `std::span<T, Extent>`.
//
//  What can be a non-type parameter: integers, enums, pointers and references
//  to objects with static storage duration (C++17 dropped the old requirement
//  that they also have linkage), `std::nullptr_t`, and -- new in C++20 --
//  FLOATING POINT and "structural" class types, which is what makes
//  compile-time strings work:
//
//      template <FixedString Name> struct Tagged;
//      Tagged<"width"> w;      // C++20
//
//  A structural type is roughly: all members public, all of them structural,
//  no user-provided copy/destructor. The compiler needs to compare two of them
//  for template-identity, which is why the restriction exists.
//
//  What this buys: values the optimiser can see through, dimensions checked at
//  compile time, and units or tags that cost nothing at run time.
//
//  TASK
//    Finish `Vector<N>` so that mismatched dimensions are a compile error, and
//    implement `FixedString` so a string literal can be a template argument.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 08_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

// A fixed-dimension mathematical vector.
template <std::size_t N>
class Vector {
public:
  constexpr Vector() = default;

  // TODO: a variadic constructor so `Vector{1.0, 2.0, 3.0}` works, with a
  // static_assert that the number of arguments matches N.

  [[nodiscard]] constexpr double operator[](std::size_t index) const {
    return values_[index];
  }
  [[nodiscard]] constexpr double& operator[](std::size_t index) {
    return values_[index];
  }

  [[nodiscard]] static constexpr std::size_t size() noexcept {
    return N;
  }

  // TODO: dot product. Because N is part of the type, a caller cannot pass a
  // Vector of a different length -- there is no run-time check to write and no
  // way to get it wrong.
  [[nodiscard]] constexpr double dot(const Vector& other) const {
    return 0.0;
  }

private:
  std::array<double, N> values_{};
};

// TODO: a deduction guide so `Vector{1.0, 2.0}` deduces Vector<2>.

// A string usable as a template argument. The members must be public and the
// type must have no user-provided copy or destructor -- that is what makes it
// "structural".
//
// TODO: implement it.
//   * `data` is a std::array<char, N>;
//   * a consteval constructor from a `const char (&)[N]` string literal;
//   * a `view()` returning a std::string_view over the characters, without the
//     trailing null.
template <std::size_t N>
struct FixedString {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
  constexpr FixedString(const char (&)[N]) {}
};

// TODO: deduction guide for FixedString.

// A value tagged with a compile-time name. The name costs nothing at run time
// -- it is part of the type.
template <FixedString Name, typename T>
struct Tagged {
  T value{};

  [[nodiscard]] static constexpr std::string_view name() {
    return Name.view();
  }
};

TEST_CASE("the dimension is part of the type") {
  constexpr Vector<3> a{1.0, 2.0, 3.0};
  static_assert(Vector<3>::size() == 3);
  static_assert(a[1] == 2.0);

  constexpr Vector<3> b{4.0, 5.0, 6.0};
  static_assert(a.dot(b) == 32.0);

  // Mixing dimensions does not compile -- uncomment to see:
  //
  //   constexpr Vector<2> c{1.0, 2.0};
  //   (void)a.dot(c);

  CHECK(a.dot(b) == doctest::Approx(32.0));
}

TEST_CASE("CTAD deduces the dimension from the arguments") {
  constexpr Vector v{1.0, 2.0};
  static_assert(Vector<2>::size() == 2);
  static_assert(std::is_same_v<decltype(v), const Vector<2>>);
  CHECK(v[0] == doctest::Approx(1.0));
}

TEST_CASE("a string literal as a template argument") {
  constexpr FixedString name{"width"};
  static_assert(name.view() == "width");
  static_assert(name.view().size() == 5);
  CHECK(true);
}

TEST_CASE("tagging a value with a compile-time name") {
  constexpr Tagged<"width", int> width{1920};
  constexpr Tagged<"height", int> height{1080};

  static_assert(decltype(width)::name() == "width");
  static_assert(decltype(height)::name() == "height");

  // Different names make different types, so these cannot be confused.
  static_assert(!std::is_same_v<decltype(width), decltype(height)>);

  // ...and the name costs nothing.
  static_assert(sizeof(width) == sizeof(int));

  CHECK(width.value == 1920);
}
