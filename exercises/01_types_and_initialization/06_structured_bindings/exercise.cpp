// =============================================================================
//  01.06 -- Structured bindings
// =============================================================================
//
//  `auto [a, b] = expr;` decomposes a pair, a tuple, an array or a plain
//  struct into named variables. It replaces a whole genre of noise:
//
//      // before
//      for (const auto& entry : scores) {
//        const std::string& name = entry.first;
//        int score = entry.second;
//      }
//
//      // after
//      for (const auto& [name, score] : scores) { ... }
//
//  Names beat `.first` and `.second`. That is most of the value.
//
//  The `auto` part deduces exactly as it does anywhere else, so the same rules
//  apply: `auto [a, b]` copies the whole object first, `auto& [a, b]` binds to
//  it, `const auto& [a, b]` binds without copying.
//
//  One thing they pair beautifully with is `map::insert`, which returns
//  `std::pair<iterator, bool>` -- a return type that is unreadable without
//  them.
//
//  TASK
//    Rewrite the three functions below using structured bindings. Each one
//    currently uses `.first` / `.second` or an explicit copy.
//
//  RUN IT
//    ./mcpp test 01_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

struct Measurement {
  std::string sensor;
  double value = 0.0;
  bool valid = false;
};

// Each entry contributes `score` multiplied by the length of its name.
//
// TODO: rewrite the loop as `for (const auto& [name, score] : scores)`. The
// bug below is the one `.first` / `.second` invites: the weight was never
// applied, and nothing in the expression says which half is which.
int total_score(const std::map<std::string, int>& scores) {
  int total = 0;
  for (const auto& entry : scores) {
    total += entry.second;
  }
  return total;
}

// Inserts a name into a set, reporting whether it was new.
//
// `std::set::insert` returns `std::pair<iterator, bool>`. Written out longhand
// that return type tells you nothing; `.second` at the call site tells you
// even less.
//
// TODO: rewrite the body as
//     const auto [position, inserted] = seen.insert(name);
// and return `inserted`. The version below reaches for `.first` -- the
// iterator, which is never `end()` after an insert -- and so reports every
// name as new.
bool remember(std::set<std::string>& seen, const std::string& name) {
  const std::pair<std::set<std::string>::iterator, bool> result = seen.insert(name);
  return result.first != seen.end();
}

// Scales the `value` of every valid measurement in place.
// TODO: rewrite with `auto& [sensor, value, valid]`. Structured bindings work
// on any plain struct, not just pairs and tuples -- and binding by reference
// lets you write through them. While you are here, notice that the `valid`
// flag is currently ignored.
void scale_valid(std::vector<Measurement>& measurements, double factor) {
  for (auto& measurement : measurements) {
    measurement.value *= factor;
  }
}

TEST_CASE("total_score sums the values") {
  // "ada" is 3 characters and scores 10; "alan" is 4 and scores 7.
  const std::map<std::string, int> scores = {{"ada", 10}, {"alan", 7}};
  CHECK(total_score(scores) == 3 * 10 + 4 * 7);
  CHECK(total_score({}) == 0);
}

TEST_CASE("remember reports whether the name was new") {
  std::set<std::string> seen;
  CHECK(remember(seen, "ada"));
  CHECK_FALSE(remember(seen, "ada"));
  CHECK(remember(seen, "alan"));
  CHECK(seen.size() == 2);
}

TEST_CASE("scale_valid only touches valid measurements") {
  std::vector<Measurement> measurements = {
      {"a", 1.0, true}, {"b", 2.0, false}, {"c", 3.0, true}};
  scale_valid(measurements, 2.0);
  CHECK(measurements[0].value == doctest::Approx(2.0));
  CHECK(measurements[1].value == doctest::Approx(2.0));
  CHECK(measurements[2].value == doctest::Approx(6.0));
}
