// =============================================================================
//  13.04 -- Capstone: queries
// =============================================================================
//
//  Reading from the store. This is chapter 7's exercise, applied to something
//  that looks like real work.
//
//  The design questions it puts in front of you:
//
//   * WHAT SHOULD AN AGGREGATION TAKE? Not a std::vector -- a RANGE, so it
//     works on a container, a span, or the result of a pipeline without any
//     caller having to materialise one first (07.03). A constrained template
//     says so precisely (08.06).
//
//   * WHAT SHOULD A SELECTION RETURN? A view, so nothing is copied until a
//     caller decides to keep something -- and `auto` for the return type,
//     because view types are unspellable (01.01, 07.03).
//
//   * WHERE MUST LAZINESS STOP? `percentile` has to sort, and sorting needs
//     the values to exist. That is the honest answer: materialise where the
//     algorithm requires it and nowhere else (07.06).
//
//   * WHEN IS A FULL SORT WASTEFUL? `top_by_mean` needs the first N in order
//     and does not care about the rest -- which is what `partial_sort` is for.
//
//  TASK
//    Implement the aggregations, the selections and the two queries.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 13_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <map>
#include <numeric>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;

struct Sample {
  Timestamp at{};
  double value = 0.0;
  friend bool operator==(const Sample&, const Sample&) = default;
};

struct SeriesKey {
  std::string name;
  std::map<std::string, std::string> tags;

  friend bool operator==(const SeriesKey&, const SeriesKey&) = default;
  friend std::strong_ordering operator<=>(const SeriesKey&, const SeriesKey&) = default;

  [[nodiscard]] bool has_tag(std::string_view key, std::string_view value) const {
    const auto it = tags.find(std::string{key});
    return it != tags.end() && it->second == value;
  }
};

using Database = std::map<SeriesKey, std::vector<Sample>>;

// ---------------------------------------------------------------------------
// Aggregations. Each takes a range of samples rather than a container, so it
// works on a vector, a span, or the result of a pipeline (06.05, 07.03).
// ---------------------------------------------------------------------------

struct Summary {
  std::size_t count = 0;
  double sum = 0.0;
  double min = 0.0;
  double max = 0.0;

  [[nodiscard]] double mean() const {
    return count == 0 ? 0.0 : sum / static_cast<double>(count);
  }
};

// TODO: summarise any range of Samples -- count, sum, min and max in one
// pass. The signature is given: constrain it to ranges OF SAMPLES, so a
// mistaken call gets a message about the constraint rather than an error from
// inside the loop (08.06).
//
// Watch the empty case: min and max of nothing are not 0, they are undefined --
// which is why `count == 0` has to be handled first rather than starting from
// a sentinel.
template <std::ranges::input_range R>
  requires std::same_as<std::ranges::range_value_t<R>, Sample>
Summary summarise(R&& samples) {
  return {};
}

// TODO: the p-th percentile by NEAREST RANK: sort the values, take the one at
// ceil(p/100 * n), 1-based. Return nothing for an empty input or a p outside
// 0..100 (05.04).
//
// This is where laziness stops -- sorting needs the values to exist.
std::optional<double> percentile(const std::vector<Sample>& samples, double p) {
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// Selection. These return views (07.03), so nothing is copied until a caller
// asks for a container.
// ---------------------------------------------------------------------------

// TODO: return a VIEW of the entries whose series has this name. Note the
// by-value parameter and the init capture: the lambda outlives this call, so
// it must own what it compares against (09.02).
auto by_name(const Database& database, std::string name) {
  return database;
}

// TODO: the same, filtering on a tag.
auto with_tag(const Database& database, std::string key, std::string value) {
  return database;
}

// TODO: the samples in the half-open range [from, to). The vector is sorted by
// time, so this is a binary search (13.03).
std::vector<Sample> in_range(const std::vector<Sample>& samples, Timestamp from,
                             Timestamp to) {
  return {};
}

// ---------------------------------------------------------------------------
// The whole query, composed.
// ---------------------------------------------------------------------------

struct QueryResult {
  SeriesKey key;
  Summary summary;
};

// TODO: for every series with this name, summarise the samples in the window.
// Series with nothing in the window are left out entirely.
std::vector<QueryResult> query(const Database& database, std::string_view name,
                               Timestamp from, Timestamp to) {
  return {};
}

// TODO: the `count` series with the highest mean, highest first. Skip empty
// series.
//
// Use std::partial_sort rather than a full sort: only the first `count` need
// to be in order, and saying so is both faster and a clearer statement of what
// the function actually needs.
std::vector<QueryResult> top_by_mean(const Database& database, std::size_t count) {
  return {};
}

namespace {

Timestamp at_ms(int milliseconds) {
  return Timestamp{std::chrono::milliseconds{milliseconds}};
}

Database sample_database() {
  return {
      {{"cpu.load", {{"host", "a"}}},
       {{at_ms(100), 0.1}, {at_ms(200), 0.3}, {at_ms(300), 0.5}}},
      {{"cpu.load", {{"host", "b"}}},
       {{at_ms(100), 0.9}, {at_ms(200), 0.7}, {at_ms(300), 0.8}}},
      {{"mem.used", {{"host", "a"}}}, {{at_ms(100), 1000.0}, {at_ms(300), 2000.0}}},
      {{"disk.free", {}}, {}},
  };
}

} // namespace

TEST_CASE("summarise works on any range of samples") {
  const std::vector<Sample> samples = {{at_ms(1), 2.0}, {at_ms(2), 8.0}, {at_ms(3), 5.0}};

  const Summary summary = summarise(samples);
  CHECK(summary.count == 3);
  CHECK(summary.sum == doctest::Approx(15.0));
  CHECK(summary.min == doctest::Approx(2.0));
  CHECK(summary.max == doctest::Approx(8.0));
  CHECK(summary.mean() == doctest::Approx(5.0));

  const Summary empty = summarise(std::vector<Sample>{});
  CHECK(empty.count == 0);
  CHECK(empty.mean() == doctest::Approx(0.0));
}

TEST_CASE("summarise accepts a view, not just a vector") {
  const std::vector<Sample> samples = {
      {at_ms(1), 1.0}, {at_ms(2), 100.0}, {at_ms(3), 3.0}};

  // A filtered view is a range, so it goes straight in -- no intermediate
  // container.
  const Summary small = summarise(
      samples | std::views::filter([](const Sample& s) { return s.value < 50.0; }));

  CHECK(small.count == 2);
  CHECK(small.sum == doctest::Approx(4.0));
}

TEST_CASE("percentiles") {
  std::vector<Sample> samples;
  for (int i = 1; i <= 100; ++i) {
    samples.push_back({at_ms(i), static_cast<double>(i)});
  }

  CHECK(percentile(samples, 50).value() == doctest::Approx(50.0));
  CHECK(percentile(samples, 95).value() == doctest::Approx(95.0));
  CHECK(percentile(samples, 100).value() == doctest::Approx(100.0));
  CHECK(percentile(samples, 0).value() == doctest::Approx(1.0));

  CHECK_FALSE(percentile({}, 50).has_value());
  CHECK_FALSE(percentile(samples, 101).has_value());
}

TEST_CASE("selecting by name") {
  const Database database = sample_database();

  int count = 0;
  for (const auto& [key, samples] : by_name(database, "cpu.load")) {
    CHECK(key.name == "cpu.load");
    ++count;
  }
  CHECK(count == 2);

  count = 0;
  for ([[maybe_unused]] const auto& entry : by_name(database, "nothing")) {
    ++count;
  }
  CHECK(count == 0);
}

TEST_CASE("selecting by tag") {
  const Database database = sample_database();

  int count = 0;
  for (const auto& [key, samples] : with_tag(database, "host", "a")) {
    CHECK(key.tags.at("host") == "a");
    ++count;
  }
  // cpu.load{host=a} and mem.used{host=a}.
  CHECK(count == 2);
}

TEST_CASE("a query over a time window") {
  const Database database = sample_database();
  const auto results = query(database, "cpu.load", at_ms(100), at_ms(300));

  REQUIRE(results.size() == 2);
  // Half-open, so the sample at 300 is excluded.
  CHECK(results[0].summary.count == 2);
  CHECK(results[0].summary.sum == doctest::Approx(0.4));
  CHECK(results[1].summary.sum == doctest::Approx(1.6));
}

TEST_CASE("a window that matches nothing yields nothing") {
  const Database database = sample_database();
  CHECK(query(database, "cpu.load", at_ms(1000), at_ms(2000)).empty());
  CHECK(query(database, "missing", at_ms(0), at_ms(9999)).empty());
}

TEST_CASE("ranking") {
  const Database database = sample_database();
  const auto top = top_by_mean(database, 2);

  REQUIRE(top.size() == 2);
  CHECK(top[0].key.name == "mem.used");
  CHECK(top[0].summary.mean() == doctest::Approx(1500.0));
  CHECK(top[1].key.name == "cpu.load");
  CHECK(top[1].key.tags.at("host") == "b");

  // The empty series is skipped, so asking for more than there are is fine.
  CHECK(top_by_mean(database, 100).size() == 3);
  CHECK(top_by_mean(database, 0).empty());
}
