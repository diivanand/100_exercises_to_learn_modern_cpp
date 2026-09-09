// =============================================================================
//  03.08 -- std::shared_ptr, and when not to reach for it
// =============================================================================
//
//  `std::shared_ptr<T>` keeps a reference count next to the object; the last
//  owner to go away destroys it. That solves genuinely shared ownership --
//  which is rarer than it looks.
//
//  What it costs, and why unique_ptr stays the default (Core Guidelines R.20,
//  R.21):
//
//   * two pointers wide instead of one;
//   * an atomic increment/decrement on every copy, even single-threaded;
//   * a second allocation for the control block, unless you use
//     std::make_shared, which fuses the two;
//   * the lifetime is now decided at run time, by whoever happens to let go
//     last -- which is much harder to reason about than a scope.
//
//  Reach for it when ownership is genuinely shared and its end genuinely
//  cannot be predicted: a cache handing out entries, an observer that may
//  outlive its subject, a node in a graph.
//
//  THE ALIASING TRAP: building two shared_ptrs from the same raw pointer
//  creates two independent counts, and the object is deleted twice. If you
//  need a second owner, copy an existing shared_ptr -- never re-wrap the
//  pointer.
//
//  TASK
//    Fix the three bugs in `Cache`: a double-delete, a needless second
//    allocation, and a `use_count` that never drops.
//
//  RUN IT
//    ./mcpp test 03_08
//
// =============================================================================

#include <doctest/doctest.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

struct Texture {
  explicit Texture(std::string name) : name(std::move(name)) {
    ++live_count_;
  }
  ~Texture() {
    --live_count_;
  }

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  [[nodiscard]] static int live_count() noexcept {
    return live_count_;
  }

  std::string name;

private:
  static inline int live_count_ = 0;
};

class Cache {
public:
  // TODO: three things are wrong here.
  //
  //  1. `new Texture{...}` followed by wrapping in a shared_ptr is two
  //     allocations. Use std::make_shared.
  //  2. `share` below re-wraps the raw pointer, creating a second control
  //     block. Return a copy of the stored shared_ptr instead.
  //  3. `evict` erases from the map but the raw pointer copy keeps the entry
  //     alive forever. Once (1) and (2) are fixed this disappears by itself.
  std::shared_ptr<Texture> load(const std::string& name) {
    if (const auto it = textures_.find(name); it != textures_.end()) {
      return it->second;
    }
    auto texture = std::shared_ptr<Texture>{new Texture{name}};
    textures_.emplace(name, texture);
    return texture;
  }

  // TODO: hand out another owner of an already-loaded texture.
  std::shared_ptr<Texture> share(const std::string& name) {
    Texture* raw = textures_.at(name).get();
    return std::shared_ptr<Texture>{raw};
  }

  void evict(const std::string& name) {
    textures_.erase(name);
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return textures_.size();
  }

private:
  std::map<std::string, std::shared_ptr<Texture>> textures_;
};

TEST_CASE("loading the same name twice shares one object") {
  CHECK(Texture::live_count() == 0);
  {
    Cache cache;
    const std::shared_ptr<Texture> first = cache.load("grass");
    const std::shared_ptr<Texture> second = cache.load("grass");

    CHECK(first.get() == second.get());
    CHECK(Texture::live_count() == 1);
    // The cache holds one reference, plus the two here.
    CHECK(first.use_count() == 3);
  }
  CHECK(Texture::live_count() == 0);
}

TEST_CASE("sharing adds an owner rather than a second control block") {
  Cache cache;
  const std::shared_ptr<Texture> owner = cache.load("stone");
  const std::shared_ptr<Texture> other = cache.share("stone");

  CHECK(owner.get() == other.get());
  // Two independent control blocks would each report a count of their own --
  // and would both delete the texture.
  CHECK(owner.use_count() == 3);
  CHECK(Texture::live_count() == 1);
}

TEST_CASE("evicting drops the cache's reference") {
  CHECK(Texture::live_count() == 0);
  Cache cache;
  {
    const std::shared_ptr<Texture> held = cache.load("water");
    CHECK(held.use_count() == 2);

    cache.evict("water");
    CHECK(cache.size() == 0);
    // The caller is still holding it, so it is still alive.
    CHECK(held.use_count() == 1);
    CHECK(Texture::live_count() == 1);
  }
  CHECK(Texture::live_count() == 0);
}

TEST_CASE("a shared_ptr is twice the size of a raw pointer") {
  static_assert(sizeof(std::shared_ptr<Texture>) == 2 * sizeof(Texture*));
  CHECK(true);
}
