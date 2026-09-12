// Solution -- 03.08 std::shared_ptr
#include <doctest/doctest.h>

#include <cstddef>
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

  static int live_count() noexcept {
    return live_count_;
  }

  std::string name;

private:
  static inline int live_count_ = 0;
};

class Cache {
public:
  std::shared_ptr<Texture> load(const std::string& name) {
    if (const auto it = textures_.find(name); it != textures_.end()) {
      return it->second;
    }
    // One allocation for the object and its control block together.
    auto texture = std::make_shared<Texture>(name);
    textures_.emplace(name, texture);
    return texture;
  }

  // Copying an existing shared_ptr joins its ownership group. Re-wrapping the
  // raw pointer would start a second, fatal one.
  std::shared_ptr<Texture> share(const std::string& name) {
    return textures_.at(name);
  }

  void evict(const std::string& name) {
    textures_.erase(name);
  }

  std::size_t size() const noexcept {
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
    CHECK(first.use_count() == 3);
  }
  CHECK(Texture::live_count() == 0);
}

TEST_CASE("sharing adds an owner rather than a second control block") {
  Cache cache;
  const std::shared_ptr<Texture> owner = cache.load("stone");
  const std::shared_ptr<Texture> other = cache.share("stone");

  CHECK(owner.get() == other.get());
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
    CHECK(held.use_count() == 1);
    CHECK(Texture::live_count() == 1);
  }
  CHECK(Texture::live_count() == 0);
}

TEST_CASE("a shared_ptr is twice the size of a raw pointer") {
  static_assert(sizeof(std::shared_ptr<Texture>) == 2 * sizeof(Texture*));
  CHECK(true);
}
