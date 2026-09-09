// =============================================================================
//  13.01 -- Capstone: value types
// =============================================================================
//
//  The last five exercises build one thing: a small in-memory metrics store,
//  the sort of component that sits behind a `/metrics` endpoint. Each one uses
//  what the earlier chapters taught, and the tests are the specification.
//
//      13.01  the value types                  <- you are here
//      13.02  parsing text into them
//      13.03  the store, and its invariants
//      13.04  queries, as range pipelines
//      13.05  concurrent ingestion
//
//  This one is about DESIGNING TYPES. Nothing here is difficult in isolation;
//  the exercise is choosing well, and the tests check the choices:
//
//    * a strongly typed name, so it cannot be confused with any other string
//      (04.02 explicit);
//    * an aggregate for the data-carrying struct, with defaults, so designated
//      initialisers work (02.05);
//    * defaulted comparisons rather than six hand-written operators (04.08);
//    * the rule of zero throughout (03.04) -- no special members anywhere;
//    * a std::formatter specialisation, so these types print (12.01).
//
//  TASK
//    Write the five types. Work down the tests: each TEST_CASE states one
//    requirement.
//
//  NOTE  This exercise starts as a compile error, as the whole chapter does.
//
//  RUN IT
//    ./mcpp test 13_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <chrono>
#include <compare>
#include <cstdint>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// TODO: a strongly typed metric name.
//
//   * an explicit constructor from a std::string (04.02) -- a bare string must
//     not convert to one;
//   * it throws std::invalid_argument for an empty name -- the name comes from
//     outside the program, so bad input is data, not a bug (05.08);
//   * a `value()` accessor;
//   * defaulted == and <=> (04.08).
class MetricName {
public:
private:
  std::string value_;
};

// A timestamp. system_clock because these are wall-clock instants that get
// serialised, not durations being measured (12.02).
using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;

// TODO: one observation -- a timestamp and a value.
//
// Keep it an AGGREGATE (no user-declared constructors), give both members
// defaults, and default == and <=>. Ordering by time and then by value falls
// out of declaration order.
//
// One subtlety: the ordering category. A double can be NaN, and every
// comparison with NaN is unordered -- so what does a defaulted <=> return for
// a struct containing one? The tests will tell you if you guess wrong.
struct Sample {
  Timestamp at{};
  double value = 0.0;
};

// TODO: a set of key/value tags, backed by a std::map (which keeps them
// sorted -- 06.06).
//
//   * a default constructor, and one from an initializer_list of pairs;
//   * `set(key, value)` inserting or overwriting, returning *this so calls
//     chain -- and taking its arguments by value to move (02.06, 06.07);
//   * `get(key)` returning a std::string_view, empty when absent -- and note
//     that it must NOT insert, so no operator[] (06.06);
//   * `contains`, `size`, `empty`, `begin`, `end`;
//   * defaulted ==.
//
// Declare no destructor, no copy and no move: the rule of zero (03.04). The
// tests check that the compiler's versions exist and are noexcept.
class Tags {
public:
private:
  std::map<std::string, std::string> entries_;
};

// TODO: a series -- a name and its tags. An aggregate, with defaulted ==.
struct Series {
  MetricName name;
  Tags tags;
};

// TODO: make MetricName and Series formattable (12.01).
//
// A bare series prints as its name; a tagged one as
//     cpu.load{core=0,host=a}
// with the tags in sorted order -- which they already are, because Tags is a
// std::map.
//
// Inherit from std::formatter<std::string> so that width and alignment specs
// work without any extra code.

namespace {

Timestamp at_ms(std::int64_t milliseconds) {
  return Timestamp{std::chrono::milliseconds{milliseconds}};
}

} // namespace

TEST_CASE("a metric name is not a string") {
  const MetricName name{"http.requests"};
  CHECK(name.value() == "http.requests");

  // The conversion has to be asked for, so a bare string cannot wander into a
  // parameter expecting a name.
  static_assert(!std::is_convertible_v<std::string, MetricName>);
  static_assert(std::is_constructible_v<MetricName, std::string>);

  CHECK_THROWS_AS(MetricName{""}, std::invalid_argument);
}

TEST_CASE("names compare and order") {
  CHECK(MetricName{"a"} == MetricName{"a"});
  CHECK(MetricName{"a"} != MetricName{"b"});
  CHECK(MetricName{"a"} < MetricName{"b"});
  CHECK(MetricName{"b"} >= MetricName{"a"});
}

TEST_CASE("a sample is an aggregate with sensible defaults") {
  const Sample empty{};
  CHECK(empty.value == doctest::Approx(0.0));

  const Sample sample{.at = at_ms(1000), .value = 42.5};
  CHECK(sample.value == doctest::Approx(42.5));
  CHECK(sample.at == at_ms(1000));

  CHECK(Sample{at_ms(1), 1.0} < Sample{at_ms(2), 0.0});
  CHECK(Sample{at_ms(1), 1.0} < Sample{at_ms(1), 2.0});
  CHECK(Sample{at_ms(1), 1.0} == Sample{at_ms(1), 1.0});
}

TEST_CASE("tags follow the rule of zero") {
  static_assert(std::is_copy_constructible_v<Tags>);
  static_assert(std::is_nothrow_move_constructible_v<Tags>);
  static_assert(std::is_copy_assignable_v<Tags>);

  Tags tags{{"host", "web-1"}, {"region", "eu"}};
  CHECK(tags.size() == 2);
  CHECK(tags.get("host") == "web-1");
  CHECK(tags.get("missing").empty());
  CHECK(tags.contains("region"));

  // set() chains, and overwrites.
  tags.set("host", "web-2").set("env", "prod");
  CHECK(tags.get("host") == "web-2");
  CHECK(tags.size() == 3);
}

TEST_CASE("copies are independent") {
  Tags original{{"a", "1"}};
  Tags copy = original;
  copy.set("a", "2");

  CHECK(original.get("a") == "1");
  CHECK(copy.get("a") == "2");
  CHECK(original != copy);
}

TEST_CASE("a series formats readably") {
  const Series bare{MetricName{"cpu.load"}, {}};
  CHECK(std::format("{}", bare) == "cpu.load");

  const Series tagged{MetricName{"cpu.load"}, Tags{{"host", "a"}, {"core", "0"}}};
  // The tags come out in sorted order, because Tags is backed by a std::map.
  CHECK(std::format("{}", tagged) == "cpu.load{core=0,host=a}");

  CHECK(std::format("{:>12}", bare) == "    cpu.load");
}
