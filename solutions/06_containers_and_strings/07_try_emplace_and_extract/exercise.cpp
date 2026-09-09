// Solution -- 06.07 try_emplace, insert_or_assign, extract, merge
#include <doctest/doctest.h>

#include <map>
#include <string>
#include <utility>

struct Payload {
  explicit Payload(std::string text) : text(std::move(text)) {
    ++constructions;
  }

  std::string text;
  static inline int constructions = 0;
};

using Registry = std::map<std::string, Payload>;

bool register_once(Registry& registry, const std::string& key, std::string text) {
  // The arguments are only forwarded to Payload's constructor if the insert
  // actually happens.
  const auto [position, inserted] = registry.try_emplace(key, std::move(text));
  return inserted;
}

bool set(Registry& registry, const std::string& key, std::string text) {
  const auto [position, inserted] =
      registry.insert_or_assign(key, Payload{std::move(text)});
  return inserted;
}

bool rename(Registry& registry, const std::string& from, const std::string& to) {
  if (registry.contains(to)) {
    return false;
  }

  auto node = registry.extract(from);
  if (node.empty()) {
    return false;
  }

  // The key of a detached node is mutable -- this is the only way to change a
  // map key in place, and the value is never copied or moved.
  node.key() = to;
  registry.insert(std::move(node));
  return true;
}

void absorb(Registry& target, Registry& source) {
  target.merge(source);
}

TEST_CASE("try_emplace does not construct when the key is taken") {
  Payload::constructions = 0;
  Registry registry;

  CHECK(register_once(registry, "a", "first"));
  CHECK(Payload::constructions == 1);

  CHECK_FALSE(register_once(registry, "a", "second"));
  CHECK(Payload::constructions == 1);
  CHECK(registry.at("a").text == "first");
}

TEST_CASE("insert_or_assign overwrites") {
  Registry registry;

  CHECK(set(registry, "a", "first"));
  CHECK(registry.at("a").text == "first");

  CHECK_FALSE(set(registry, "a", "second"));
  CHECK(registry.at("a").text == "second");
  CHECK(registry.size() == 1);
}

TEST_CASE("extract renames without touching the value") {
  Registry registry;
  register_once(registry, "old", "payload");
  Payload::constructions = 0;

  CHECK(rename(registry, "old", "new"));
  CHECK(Payload::constructions == 0);
  CHECK(registry.contains("new"));
  CHECK_FALSE(registry.contains("old"));
  CHECK(registry.at("new").text == "payload");

  CHECK_FALSE(rename(registry, "missing", "other"));
  register_once(registry, "taken", "x");
  CHECK_FALSE(rename(registry, "new", "taken"));
}

TEST_CASE("merge moves what it can and leaves the rest") {
  Registry target;
  register_once(target, "shared", "target's");
  register_once(target, "only-target", "t");

  Registry source;
  register_once(source, "shared", "source's");
  register_once(source, "only-source", "s");

  Payload::constructions = 0;
  absorb(target, source);
  CHECK(Payload::constructions == 0);

  CHECK(target.size() == 3);
  CHECK(target.at("shared").text == "target's");
  CHECK(target.at("only-source").text == "s");

  CHECK(source.size() == 1);
  CHECK(source.at("shared").text == "source's");
}
