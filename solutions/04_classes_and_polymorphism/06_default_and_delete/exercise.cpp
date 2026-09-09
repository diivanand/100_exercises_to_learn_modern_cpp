// Solution -- 04.06 = default and = delete
#include <doctest/doctest.h>

#include <string>
#include <type_traits>
#include <utility>

struct Vec2 {
  // Defaulted, not user-provided: Vec2 stays trivially default constructible,
  // and `Vec2{}` value-initialises the members to zero.
  Vec2() = default;
  constexpr Vec2(double x, double y) : x(x), y(y) {}

  double x;
  double y;
};

class Handle {
public:
  Handle() = default;
  explicit Handle(int slot) : slot_(slot) {}

  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  Handle(Handle&&) = default;
  Handle& operator=(Handle&&) = default;
  ~Handle() = default;

  [[nodiscard]] int slot() const noexcept {
    return slot_;
  }

private:
  int slot_ = -1;
};

std::string schedule(int milliseconds) {
  return "in " + std::to_string(milliseconds) + "ms";
}

// Chosen by overload resolution for any floating-point argument, then
// rejected. Without it, `schedule(0.5)` converts to int and silently means
// something else.
template <typename T>
  requires std::is_floating_point_v<T>
std::string schedule(T) = delete;

TEST_CASE("Vec2 is a trivial, constexpr-friendly value type") {
  static_assert(std::is_trivially_copyable_v<Vec2>);
  static_assert(std::is_trivially_default_constructible_v<Vec2>);

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
}
