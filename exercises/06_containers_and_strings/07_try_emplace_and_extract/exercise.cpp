// =============================================================================
//  06.07 -- try_emplace, insert_or_assign, extract, merge
// =============================================================================
//
//  C++17 added four associative-container operations that fix long-standing
//  awkwardness. All four are on both map and unordered_map.
//
//    try_emplace(key, args...)
//        Insert only if the key is absent. Unlike `emplace`, it does NOT
//        construct the value (and does not move from your arguments) when the
//        key is already there. That is the bug `emplace` has: it may consume a
//        moved-from argument and then not insert anything.
//
//    insert_or_assign(key, value)
//        Insert, or overwrite the existing value. Returns whether it inserted.
//        Unlike `m[key] = value`, it does not require the value type to be
//        default-constructible.
//
//    extract(key)
//        Detach a node from the container and hand it to you. The key becomes
//        modifiable -- this is the only way to change a key without a copy --
//        and re-inserting it moves nothing.
//
//    merge(other)
//        Move every node that does not collide out of `other`. No allocation.
//
//  TASK
//    Implement the four functions. Each one is a place where the old idiom
//    either allocates unnecessarily or is subtly wrong.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 06_07
//
// =============================================================================

#include <doctest/doctest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

// A value that is expensive to build and counts how often it was built.
struct Payload {
  explicit Payload(std::string text) : text(std::move(text)) {
    ++constructions;
  }

  std::string text;
  static inline int constructions = 0;
};

using Registry = std::map<std::string, Payload>;

// Registers `text` under `key` only if the key is free. Returns whether it
// inserted.
//
// TODO: use try_emplace. `emplace` would construct a Payload even when the key
// is taken -- which is what the test counts.
bool register_once(Registry& registry, const std::string& key, std::string text) {
  return false;
}

// Sets `key` to `text`, whether or not it was there.
//
// TODO: use insert_or_assign. `registry[key] = Payload{text}` would not
// compile here at all: Payload has no default constructor, and operator[]
// needs one.
bool set(Registry& registry, const std::string& key, std::string text) {
  return false;
}

// Renames a key, keeping the same value object. Returns false if `from` is
// missing or `to` is taken.
//
// TODO: use extract. Erasing and re-inserting would move or copy the Payload;
// extract moves the node, so the value is never touched.
bool rename(Registry& registry, const std::string& from, const std::string& to) {
  return false;
}

// Moves everything from `source` into `target` that does not collide, leaving
// the collisions behind in `source`.
//
// TODO: use merge -- one call, no allocations, and `source` is left holding
// exactly the entries that could not move.
void absorb(Registry& target, Registry& source) {}

TEST_CASE("try_emplace does not construct when the key is taken") {
  Payload::constructions = 0;
  Registry registry;

  CHECK(register_once(registry, "a", "first"));
  CHECK(Payload::constructions == 1);

  // The key is taken, so nothing should be built.
  CHECK_FALSE(register_once(registry, "a", "second"));
  CHECK(Payload::constructions == 1);
  CHECK(registry.at("a").text == "first");
}

TEST_CASE("insert_or_assign overwrites") {
  Registry registry;

  CHECK(set(registry, "a", "first"));
  CHECK(registry.at("a").text == "first");

  // Returns false because it assigned rather than inserted.
  CHECK_FALSE(set(registry, "a", "second"));
  CHECK(registry.at("a").text == "second");
  CHECK(registry.size() == 1);
}

TEST_CASE("extract renames without touching the value") {
  Registry registry;
  register_once(registry, "old", "payload");
  Payload::constructions = 0;

  CHECK(rename(registry, "old", "new"));
  CHECK(Payload::constructions == 0); // the node moved; nothing was rebuilt
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

  // The colliding entry stays behind.
  CHECK(source.size() == 1);
  CHECK(source.at("shared").text == "source's");
}
