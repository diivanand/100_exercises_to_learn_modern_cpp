// =============================================================================
//  01.01 -- auto, and the reference you forgot to ask for
// =============================================================================
//
//  `auto` deduces a type the same way a function template parameter does, and
//  that has one consequence that bites everybody exactly once:
//
//      auto x = expr;   // top-level const and reference are STRIPPED
//      auto& x = expr;  // binds to the object itself
//      const auto& x;   // binds without copying, promises not to modify
//      auto&& x = expr; // forwarding reference: binds to anything
//
//  So `for (auto value : values)` gives you a *copy* of each element on every
//  iteration. Writing to it modifies the copy and then throws it away. The
//  compiler is happy; your data is unchanged.
//
//  When to use `auto` at all (Core Guidelines ES.11): when the type is
//  obvious from the right-hand side, unutterable (a lambda), or so long that
//  spelling it out hurts more than it helps -- iterators being the classic
//  case. Prefer to spell out the type when it is the point of the line.
//
//  TASK
//    Fix `shout` so it actually modifies the vector's elements, and fix
//    `longest` so it does not copy every string it inspects.
//
//  RUN IT
//    ./mcpp test 01_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <vector>

void shout(std::vector<std::string>& words) {
  // TODO: this compiles, runs, and does nothing. `word` is a fresh std::string
  // on every iteration -- a copy that is modified and then destroyed at the
  // closing brace. Ask for a reference to the element instead.
  for (std::string& word : words) {
    word += '!';
  }
}

// Returns a view of the longest string. Ties go to the first one seen.
std::string_view longest(const std::vector<std::string>& words) {
  std::string_view best;
  // TODO: `auto word` copies each std::string -- an allocation per element,
  // for a loop that only reads. Take a `const auto&` instead.
  //
  // (Once you do, notice that `best` is still safe: it views characters owned
  // by `words`, which outlives the call.)
  for (const auto& word : words) {
    if (word.size() > best.size()) {
      best = word;
    }
  }
  return best;
}

TEST_CASE("shout modifies the vector in place") {
  std::vector<std::string> words = {"go", "fast"};
  shout(words);
  CHECK(words == std::vector<std::string>{"go!", "fast!"});
}

TEST_CASE("longest returns a view into the original strings") {
  const std::vector<std::string> words = {"a", "three", "of", "elephant", "cat"};
  CHECK(longest(words) == "elephant");

  // The returned view must point *into* the vector's own storage, not at a
  // copy that has already been destroyed. Comparing addresses proves it.
  CHECK(longest(words).data() == words[3].data());
}

TEST_CASE("longest handles the empty case") {
  const std::vector<std::string> none;
  CHECK(longest(none).empty());
}
