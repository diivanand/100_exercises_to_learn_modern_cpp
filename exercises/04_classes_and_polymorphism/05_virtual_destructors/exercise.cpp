// =============================================================================
//  04.05 -- Destructors in a hierarchy
// =============================================================================
//
//  `delete base_pointer;` where the object is really a Derived, and Base's
//  destructor is not virtual, is undefined behaviour. In practice: the derived
//  destructor never runs, its members leak, and nothing warns you.
//
//  The rule (Core Guidelines C.35) is sharper than "always make destructors
//  virtual":
//
//      A base class destructor should be either PUBLIC AND VIRTUAL,
//      or PROTECTED AND NON-VIRTUAL.
//
//   * public + virtual -- "you may delete me through a Base*". Costs a vtable
//     entry, which you are already paying for if you have any virtual function.
//
//   * protected + non-virtual -- "you may NOT delete me through a Base*".
//     Derived classes can still destroy their base subobject. This is right
//     for interfaces held by value elsewhere, or by a shared_ptr (which
//     remembers the real deleter regardless).
//
//  A related trap: declaring a destructor -- even `= default` -- suppresses
//  the implicit move operations (03.04). A polymorphic base therefore usually
//  declares all five, which is why the classes below are so verbose. That
//  verbosity is the price of polymorphism, and a reason to prefer composition
//  where you can.
//
//  TASK
//    Fix the two hierarchies: `Resource` is deleted through a base pointer,
//    and `Mixin` never should be.
//
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 04_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// ---------------------------------------------------------------------------
// Hierarchy 1: owned polymorphically, so deletion happens through Resource*.
// ---------------------------------------------------------------------------

class Resource {
public:
  Resource() {
    ++live_count;
  }

  // TODO: this destructor must be virtual -- `std::unique_ptr<Resource>`
  // deletes through a Resource*, and without `virtual` the derived
  // destructor never runs.
  ~Resource() {
    --live_count;
  }

  Resource(const Resource&) = delete;
  Resource& operator=(const Resource&) = delete;

  [[nodiscard]] virtual std::string kind() const = 0;

  static inline int live_count = 0;
};

class Buffer : public Resource {
public:
  explicit Buffer(std::size_t size) : bytes_(size, 0) {
    ++buffer_live_count;
  }
  ~Buffer() override {
    --buffer_live_count;
  }

  [[nodiscard]] std::string kind() const override {
    return "buffer";
  }

  static inline int buffer_live_count = 0;

private:
  std::vector<unsigned char> bytes_;
};

// ---------------------------------------------------------------------------
// Hierarchy 2: a policy mixin. Nobody should ever hold one of these by
// pointer, let alone delete one.
// ---------------------------------------------------------------------------

class Mixin {
public:
  // TODO: make this destructor protected and non-virtual, so that deleting
  // through a Mixin* stops compiling instead of quietly misbehaving. A mixin
  // has no vtable and should not grow one.
  virtual ~Mixin() = default;

  [[nodiscard]] int tag() const noexcept {
    return 7;
  }
};

class Widget : public Mixin {
public:
  [[nodiscard]] std::string name() const {
    return "widget";
  }
};

TEST_CASE("deleting through a base pointer runs the derived destructor") {
  CHECK(Resource::live_count == 0);
  CHECK(Buffer::buffer_live_count == 0);
  {
    const std::unique_ptr<Resource> resource = std::make_unique<Buffer>(1024);
    CHECK(resource->kind() == "buffer");
    CHECK(Resource::live_count == 1);
    CHECK(Buffer::buffer_live_count == 1);
  }
  CHECK(Resource::live_count == 0);
  // This is the assertion that fails without a virtual destructor: ~Buffer
  // was never called, so its bytes_ vector was never freed.
  CHECK(Buffer::buffer_live_count == 0);
}

TEST_CASE("a polymorphic base has a virtual destructor") {
  static_assert(std::has_virtual_destructor_v<Resource>);
  CHECK(true);
}

TEST_CASE("a mixin cannot be deleted through the base") {
  static_assert(!std::has_virtual_destructor_v<Mixin>);

  // With a protected destructor this line does not compile, which is the
  // point -- the mistake is caught rather than tolerated:
  //
  //   Mixin* leaky = new Widget{};
  //   delete leaky;

  const Widget widget;
  CHECK(widget.tag() == 7);
  CHECK(widget.name() == "widget");
}

TEST_CASE("a non-polymorphic mixin stays small") {
  // No vtable pointer: an empty base contributes nothing to the object.
  static_assert(sizeof(Widget) == 1);
  CHECK(true);
}
