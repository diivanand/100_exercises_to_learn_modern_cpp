// =============================================================================
//  04.04 -- override and final
// =============================================================================
//
//  Overriding a virtual function requires the signature to match EXACTLY. Get
//  one `const` wrong and you have not overridden anything -- you have declared
//  a new, unrelated function that happens to share a name, and the base
//  version keeps being called:
//
//      class Base    { virtual void draw(int scale) const; };
//      class Derived { void draw(int scale); };   // NOT an override. Silent.
//
//  `override` asks the compiler to check:
//
//      void draw(int scale) override;   // error: does not override anything
//
//  It costs nothing and catches a whole family of bugs at the moment they are
//  introduced. Core Guidelines C.128: "Virtual functions should specify
//  exactly one of virtual, override, or final".
//
//  `final` closes the door: on a function, no further overrides; on a class,
//  no further derivation. Use it when the invariant genuinely requires it --
//  and note that a `final` class also lets the compiler devirtualise calls.
//
//  Two more rules from the same family:
//
//   * `virtual` on the base declaration only. Repeating it on the override is
//     noise, and `override` already says the function is virtual.
//   * An override that changes the default argument is a trap -- default
//     arguments are resolved statically, the function dynamically.
//
//  TASK
//    Add `override` everywhere it belongs. Two of the derived functions do
//    not actually override anything; the compiler will tell you which, and
//    then you fix the signatures.
//
//  NOTE  This exercise starts as a compile error once you add `override` --
//        and the errors are the exercise.
//
//  RUN IT
//    ./mcpp test 04_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <memory>
#include <numbers>
#include <string>
#include <type_traits>
#include <vector>

class Shape {
public:
  Shape() = default;
  Shape(const Shape&) = default;
  Shape& operator=(const Shape&) = default;
  Shape(Shape&&) = default;
  Shape& operator=(Shape&&) = default;
  virtual ~Shape() = default;

  [[nodiscard]] virtual double area() const = 0;
  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual std::string describe(int precision) const {
    return name() + " (precision " + std::to_string(precision) + ")";
  }
};

class Circle : public Shape {
public:
  explicit Circle(double radius) : radius_(radius) {}

  // TODO: add `override` to each of these. Two of them are wrong -- one is
  // missing a `const`, one takes the wrong parameter type -- and the compiler
  // will point at exactly those once you have asked it to check.
  [[nodiscard]] double area() {
    return std::numbers::pi * radius_ * radius_;
  }
  [[nodiscard]] std::string name() const {
    return "circle";
  }
  [[nodiscard]] std::string describe(double precision) const {
    return "circle at precision " + std::to_string(static_cast<int>(precision));
  }

private:
  double radius_;
};

// A square is a rectangle whose sides cannot diverge, so nothing should
// specialise it further.
// TODO: mark this class `final`.
class Square : public Shape {
public:
  explicit Square(double side) : side_(side) {}

  [[nodiscard]] double area() const override {
    return side_ * side_;
  }
  [[nodiscard]] std::string name() const override {
    return "square";
  }

private:
  double side_;
};

double total_area(const std::vector<std::unique_ptr<Shape>>& shapes) {
  double total = 0.0;
  for (const auto& shape : shapes) {
    total += shape->area();
  }
  return total;
}

TEST_CASE("virtual dispatch reaches the derived implementations") {
  std::vector<std::unique_ptr<Shape>> shapes;
  shapes.push_back(std::make_unique<Circle>(1.0));
  shapes.push_back(std::make_unique<Square>(2.0));

  CHECK(shapes[0]->name() == "circle");
  CHECK(shapes[1]->name() == "square");
  CHECK(total_area(shapes) == doctest::Approx(std::numbers::pi + 4.0));
}

TEST_CASE("describe is overridden, not shadowed") {
  const Circle circle{1.0};
  const Shape& shape = circle;
  // If `describe` were merely hidden rather than overridden, this call would
  // reach Shape::describe and produce "circle (precision 2)".
  CHECK(shape.describe(2) == "circle at precision 2");
}

TEST_CASE("Square is final") {
  static_assert(std::is_final_v<Square>);
  static_assert(!std::is_final_v<Circle>);
  CHECK(true);
}
