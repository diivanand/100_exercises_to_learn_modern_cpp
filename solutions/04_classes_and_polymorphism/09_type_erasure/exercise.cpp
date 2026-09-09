// Solution -- 04.09 Type erasure
#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

struct Circle {
  double radius = 1.0;
  [[nodiscard]] std::string render() const {
    return "circle(" + std::to_string(static_cast<int>(radius)) + ")";
  }
};

struct Label {
  std::string text;
  [[nodiscard]] std::string render() const {
    return "label(" + text + ")";
  }
};

struct Spacer {
  [[nodiscard]] std::string render() const {
    return "spacer";
  }
};

class Drawable {
public:
  // Not explicit: at the call site a Circle should simply *be* a Drawable.
  template <typename T>
  Drawable(T object) // NOLINT(google-explicit-constructor)
      : self_(std::make_unique<Model<T>>(std::move(object))) {}

  Drawable(const Drawable& other) : self_(other.self_->clone()) {}

  Drawable& operator=(const Drawable& other) {
    if (this != &other) {
      self_ = other.self_->clone();
    }
    return *this;
  }

  Drawable(Drawable&&) noexcept = default;
  Drawable& operator=(Drawable&&) noexcept = default;
  ~Drawable() = default;

  [[nodiscard]] std::string render() const {
    return self_->render();
  }

private:
  struct Concept {
    virtual ~Concept() = default;
    Concept() = default;
    Concept(const Concept&) = default;
    Concept& operator=(const Concept&) = default;
    Concept(Concept&&) = default;
    Concept& operator=(Concept&&) = default;

    [[nodiscard]] virtual std::string render() const = 0;
    [[nodiscard]] virtual std::unique_ptr<Concept> clone() const = 0;
  };

  template <typename T>
  struct Model final : Concept {
    explicit Model(T value) : object(std::move(value)) {}

    [[nodiscard]] std::string render() const override {
      return object.render();
    }

    [[nodiscard]] std::unique_ptr<Concept> clone() const override {
      return std::make_unique<Model>(object);
    }

    T object;
  };

  std::unique_ptr<Concept> self_;
};

std::string render_all(const std::vector<Drawable>& drawables) {
  std::string result;
  for (const auto& drawable : drawables) {
    if (!result.empty()) {
      result += " ";
    }
    result += drawable.render();
  }
  return result;
}

TEST_CASE("unrelated types share one interface") {
  const std::vector<Drawable> scene = {Circle{2.0}, Label{"hi"}, Spacer{}};
  CHECK(render_all(scene) == "circle(2) label(hi) spacer");
}

TEST_CASE("a Drawable is a value, not a handle") {
  const Drawable original = Label{"first"};
  Drawable copy = original;
  copy = Circle{3.0};

  CHECK(original.render() == "label(first)");
  CHECK(copy.render() == "circle(3)");
}

TEST_CASE("copies are deep") {
  std::vector<Drawable> a = {Label{"x"}};
  const std::vector<Drawable> b = a;
  a[0] = Label{"y"};

  CHECK(a[0].render() == "label(y)");
  CHECK(b[0].render() == "label(x)");
}

TEST_CASE("adding a new type requires nothing of the old ones") {
  struct Divider {
    [[nodiscard]] std::string render() const {
      return "---";
    }
  };

  const std::vector<Drawable> scene = {Divider{}, Spacer{}};
  CHECK(render_all(scene) == "--- spacer");
}
