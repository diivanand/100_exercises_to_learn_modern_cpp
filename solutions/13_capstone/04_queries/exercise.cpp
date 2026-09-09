// Solution -- 13.04 Capstone: queries
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

template <std::ranges::input_range R>
  requires std::same_as<std::ranges::range_value_t<R>, Sample>
Summary summarise(R&& samples) {
  Summary summary;
  for (const Sample& sample : samples) {
    if (summary.count == 0) {
      summary.min = sample.value;
      summary.max = sample.value;
    } else {
      summary.min = std::min(summary.min, sample.value);
      summary.max = std::max(summary.max, sample.value);
    }
    summary.sum += sample.value;
    ++summary.count;
  }
  return summary;
}

// The p-th percentile by nearest-rank, which needs the values sorted -- so
// this one materialises (07.06). Not everything can stay lazy.
std::optional<double> percentile(const std::vector<Sample>& samples, double p) {
  if (samples.empty() || p < 0.0 || p > 100.0) {
    return std::nullopt;
  }

  std::vector<double> values;
  values.reserve(samples.size());
  for (const Sample& sample : samples) {
    values.push_back(sample.value);
  }
  std::ranges::sort(values);

  const auto rank =
      static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(values.size())));
  const std::size_t index = rank == 0 ? 0 : rank - 1;
  return values[index];
}

// ---------------------------------------------------------------------------
// Selection. These return views (07.03), so nothing is copied until a caller
// asks for a container.
// ---------------------------------------------------------------------------

auto by_name(const Database& database, std::string name) {
  return database | std::views::filter([name = std::move(name)](const auto& entry) {
           return entry.first.name == name;
         });
}

auto with_tag(const Database& database, std::string key, std::string value) {
  return database | std::views::filter([key = std::move(key),
                                        value = std::move(value)](const auto& entry) {
           return entry.first.has_tag(key, value);
         });
}

std::vector<Sample> in_range(const std::vector<Sample>& samples, Timestamp from,
                             Timestamp to) {
  const auto first = std::ranges::lower_bound(samples, from, {}, &Sample::at);
  const auto last = std::ranges::lower_bound(samples, to, {}, &Sample::at);
  return {first, last};
}

// ---------------------------------------------------------------------------
// The whole query, composed.
// ---------------------------------------------------------------------------

struct QueryResult {
  SeriesKey key;
  Summary summary;
};

std::vector<QueryResult> query(const Database& database, std::string_view name,
                               Timestamp from, Timestamp to) {
  std::vector<QueryResult> results;
  for (const auto& [key, samples] : by_name(database, std::string{name})) {
    const auto window = in_range(samples, from, to);
    if (window.empty()) {
      continue;
    }
    results.push_back({key, summarise(window)});
  }
  return results;
}

// The top `count` series by mean value, highest first.
std::vector<QueryResult> top_by_mean(const Database& database, std::size_t count) {
  std::vector<QueryResult> results;
  results.reserve(database.size());
  for (const auto& [key, samples] : database) {
    if (!samples.empty()) {
      results.push_back({key, summarise(samples)});
    }
  }

  // partial_sort: only the first `count` need to be in order, which is cheaper
  // than sorting everything when the database is large.
  const std::size_t take = std::min(count, results.size());
  std::partial_sort(results.begin(), results.begin() + static_cast<std::ptrdiff_t>(take),
                    results.end(), [](const QueryResult& a, const QueryResult& b) {
                      return a.summary.mean() > b.summary.mean();
                    });
  results.resize(take);
  return results;
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
