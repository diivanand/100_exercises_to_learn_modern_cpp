// Solution -- 04.04 override and final
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

  // `const` restored, and `describe` now takes an int -- both mistakes the
  // compiler found the moment `override` was added.
  [[nodiscard]] double area() const override {
    return std::numbers::pi * radius_ * radius_;
  }
  [[nodiscard]] std::string name() const override {
    return "circle";
  }
  [[nodiscard]] std::string describe(int precision) const override {
    return "circle at precision " + std::to_string(precision);
  }

private:
  double radius_;
};

class Square final : public Shape {
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
  CHECK(shape.describe(2) == "circle at precision 2");
}

TEST_CASE("Square is final") {
  static_assert(std::is_final_v<Square>);
  static_assert(!std::is_final_v<Circle>);
  CHECK(true);
}
