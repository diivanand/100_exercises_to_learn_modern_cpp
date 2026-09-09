// Solution -- 08.05 Non-type template parameters
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

template <std::size_t N>
class Vector {
public:
  constexpr Vector() = default;

  template <typename... Ts>
  constexpr explicit Vector(Ts... values) : values_{static_cast<double>(values)...} {
    static_assert(sizeof...(Ts) == N, "wrong number of components");
  }

  [[nodiscard]] constexpr double operator[](std::size_t index) const {
    return values_[index];
  }
  [[nodiscard]] constexpr double& operator[](std::size_t index) {
    return values_[index];
  }

  [[nodiscard]] static constexpr std::size_t size() noexcept {
    return N;
  }

  // The parameter type is `Vector<N>` -- the same N. A Vector<2> simply does
  // not convert, so the dimension check happens at compile time and costs
  // nothing at run time.
  [[nodiscard]] constexpr double dot(const Vector& other) const {
    double total = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
      total += values_[i] * other.values_[i];
    }
    return total;
  }

private:
  std::array<double, N> values_{};
};

template <typename... Ts>
Vector(Ts...) -> Vector<sizeof...(Ts)>;

template <std::size_t N>
struct FixedString {
  // consteval: a FixedString only ever exists at compile time.
  // The `const char (&)[N]` parameter is what lets N be deduced from a string
  // literal, so it cannot be a std::array here.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
  consteval FixedString(const char (&literal)[N]) {
    std::copy_n(literal, N, data.begin());
  }

  [[nodiscard]] constexpr std::string_view view() const {
    // N includes the literal's trailing null, which is not part of the string.
    return std::string_view{data.data(), N - 1};
  }

  // Public, with no user-provided copy or destructor: a structural type, and
  // therefore usable as a template argument.
  std::array<char, N> data{};
};

template <std::size_t N>
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
FixedString(const char (&)[N]) -> FixedString<N>;

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
  static_assert(!std::is_same_v<decltype(width), decltype(height)>);
  static_assert(sizeof(width) == sizeof(int));

  CHECK(width.value == 1920);
}
