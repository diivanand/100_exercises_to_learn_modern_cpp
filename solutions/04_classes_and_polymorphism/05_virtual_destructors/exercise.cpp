// Solution -- 04.05 Destructors in a hierarchy
#include <doctest/doctest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

class Resource {
public:
  Resource() {
    ++live_count;
  }

  // Public and virtual: deletion through a Resource* is part of the contract.
  virtual ~Resource() {
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

class Mixin {
public:
  [[nodiscard]] int tag() const noexcept {
    return 7;
  }

protected:
  // Protected and non-virtual: derived classes can destroy their base
  // subobject, outsiders cannot delete through a Mixin*, and no vtable is
  // created for a class that has no virtual functions.
  ~Mixin() = default;
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
  CHECK(Buffer::buffer_live_count == 0);
}

TEST_CASE("a polymorphic base has a virtual destructor") {
  static_assert(std::has_virtual_destructor_v<Resource>);
  CHECK(true);
}

TEST_CASE("a mixin cannot be deleted through the base") {
  static_assert(!std::has_virtual_destructor_v<Mixin>);

  const Widget widget;
  CHECK(widget.tag() == 7);
  CHECK(widget.name() == "widget");
}

TEST_CASE("a non-polymorphic mixin stays small") {
  static_assert(sizeof(Widget) == 1);
  CHECK(true);
}
