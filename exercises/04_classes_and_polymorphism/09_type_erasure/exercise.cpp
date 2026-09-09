// =============================================================================
//  04.09 -- Type erasure: polymorphism without inheritance
// =============================================================================
//
//  Inheritance-based polymorphism makes a demand of every participant: you
//  must derive from my base class. That is fine when you own the types. It
//  fails when you do not -- you cannot make `int` derive from `Drawable`.
//
//  Type erasure gets the same runtime dispatch without the demand. The trick
//  is one you have already seen twice in this chapter: a public value type
//  holding a unique_ptr to a private polymorphic implementation.
//
//      class Drawable {                       // public: a VALUE
//        struct Concept { virtual ... };      // private: the interface
//        template <typename T>
//        struct Model : Concept { T object; } // private: the adapter
//        std::unique_ptr<Concept> self_;
//      };
//
//  Every type that has the right operations fits, whether or not it has ever
//  heard of Drawable. `std::function`, `std::any` and `std::shared_ptr`'s
//  deleter are all built this way.
//
//  What you gain: value semantics (copy, assign, put it in a vector), no
//  inheritance requirement, no headers shared between unrelated types.
//  What you pay: an allocation and an indirect call per object -- the same
//  price virtual dispatch charges, plus the allocation.
//
//  TASK
//    Finish `Drawable`. `Model<T>` and the constructor are the interesting
//    part; the rest is scaffolding you have already met.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 04_09
//
// =============================================================================

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

// Three unrelated types. None of them inherits from anything; none of them
// knows Drawable exists. They only agree on the name and shape of `render`.
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
  // TODO: a converting constructor that accepts ANY type with a `render()`
  // member, wrapping it in a Model<T>.
  //
  //     template <typename T>
  //     Drawable(T object) : self_(std::make_unique<Model<T>>(std::move(object))) {}
  //
  // Deliberately not explicit: the whole point is that a Circle *is* a
  // Drawable at the call site.

  // TODO: copy operations. A Drawable is a value, so copying it must copy the
  // object inside -- which is what `Concept::clone` is for. Move operations
  // can be defaulted.

  [[nodiscard]] std::string render() const {
    return self_->render();
  }

private:
  // The erased interface. Only Drawable ever sees it.
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

  // TODO: the adapter. One template, instantiated once per type you wrap:
  //
  //     template <typename T>
  //     struct Model final : Concept {
  //       explicit Model(T object) : object_(std::move(object)) {}
  //       std::string render() const override { return object_.render(); }
  //       std::unique_ptr<Concept> clone() const override { ... }
  //       T object_;
  //     };

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

  // Assigning to the copy must not disturb the original.
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
  // A type declared after Drawable, in another file, in another library --
  // it still fits.
  struct Divider {
    [[nodiscard]] std::string render() const {
      return "---";
    }
  };

  const std::vector<Drawable> scene = {Divider{}, Spacer{}};
  CHECK(render_all(scene) == "--- spacer");
}
