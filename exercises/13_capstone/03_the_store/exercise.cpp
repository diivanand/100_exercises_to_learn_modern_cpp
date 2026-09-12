// =============================================================================
//  13.03 -- Capstone: the store and its invariants
// =============================================================================
//
//  Now the container the readings go into. This is the exercise about CLASS
//  DESIGN: what the invariant is, where it is established, and what the public
//  interface has to refuse in order to keep it.
//
//  SeriesData's invariant, in one sentence:
//
//      the samples are always sorted by timestamp, and there are never more
//      than `capacity` of them.
//
//  Everything follows from that:
//
//   * the vector is private, and `samples()` returns a CONST reference -- a
//     mutable one would hand the invariant away (04.01);
//   * `add` inserts in the right place rather than appending, because metrics
//     genuinely do arrive out of order;
//   * `between` can binary-search, which is the payoff for maintaining the
//     ordering (07.01);
//   * a capacity of zero would make the class meaningless, so the constructor
//     rejects it (05.08).
//
//  And in MetricStore, two habits from chapter 6 that matter here:
//   * `try_emplace`, so looking up a series does not construct one (06.07);
//   * `find`, not `operator[]`, so READING does not create an entry (06.06).
//
//  TASK
//    Implement SeriesData and MetricStore. The tests are the specification --
//    read them first; several of them exist to catch a specific shortcut.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 13_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;

struct Sample {
  Timestamp at{};
  double value = 0.0;
  friend bool operator==(const Sample&, const Sample&) = default;
};

// The key a series is stored under. Ordered, so the store iterates
// deterministically -- which matters more than the microseconds a hash would
// save (06.06).
struct SeriesKey {
  std::string name;
  std::map<std::string, std::string> tags;

  friend bool operator==(const SeriesKey&, const SeriesKey&) = default;
  friend std::strong_ordering operator<=>(const SeriesKey&, const SeriesKey&) = default;
};

// A time-ordered run of samples for one series.
//
// THE INVARIANT: samples_ is always sorted by timestamp, and holds at most
// `capacity_` of them -- the oldest are dropped first. Every public function
// preserves it; nothing outside the class can break it, because nothing
// outside can reach the vector (04.01, 05.08).
class SeriesData {
public:
  // TODO: a constructor taking the capacity, rejecting 0 with
  // std::invalid_argument.
  explicit SeriesData(std::size_t capacity) : capacity_(capacity) {}

  // TODO: insert `sample` in timestamp order, then drop the oldest if the
  // capacity is exceeded. `std::ranges::upper_bound` with a projection finds
  // where it goes (07.02).
  void add(Sample sample) {}

  // TODO: size, empty, capacity.

  // TODO: a CONST reference to the samples. Not a copy (this is called often),
  // and not a mutable reference (that would give the invariant away).
  const std::vector<Sample>& samples() const noexcept {
    return samples_;
  }

  // TODO: the most recent sample, or nothing (05.04).
  std::optional<Sample> latest() const {
    return std::nullopt;
  }

  // TODO: the samples in the half-open range [from, to). Because the vector is
  // sorted, this is two binary searches rather than a scan.
  std::vector<Sample> between(Timestamp from, Timestamp to) const {
    return {};
  }

  // TODO: remove everything before `cutoff`, returning how many went.
  std::size_t drop_before(Timestamp cutoff) {
    return 0;
  }

private:
  std::size_t capacity_;
  std::vector<Sample> samples_;
};

class MetricStore {
public:
  explicit MetricStore(std::size_t capacity_per_series = 1000)
      : capacity_per_series_(capacity_per_series) {}

  // TODO: add a sample, creating the series if this is the first one.
  // `try_emplace` builds the SeriesData only when the key is actually new.
  void record(SeriesKey key, Sample sample) {}

  // TODO: series_count, and sample_count (the total across all series).

  // TODO: look up a series WITHOUT creating it -- so `find`, not operator[].
  // Returning a pointer lets absence be expressed without an exception.
  const SeriesData* find(const SeriesKey& key) const {
    return nullptr;
  }

  // TODO: the latest sample of one series, or nothing.
  std::optional<Sample> latest(const SeriesKey& key) const {
    return std::nullopt;
  }

  // TODO: every key, in sorted order.
  std::vector<SeriesKey> keys() const {
    return {};
  }

  // TODO: drop every sample before `cutoff` across every series, returning how
  // many went -- and remove any series left with nothing in it (06.09).
  std::size_t drop_before(Timestamp cutoff) {
    return 0;
  }

private:
  std::size_t capacity_per_series_;
  std::map<SeriesKey, SeriesData> series_;
};

TEST_CASE("samples are kept in time order however they arrive") {
  SeriesData data{10};
  data.add({Timestamp{std::chrono::milliseconds{300}}, 3.0});
  data.add({Timestamp{std::chrono::milliseconds{100}}, 1.0});
  data.add({Timestamp{std::chrono::milliseconds{200}}, 2.0});

  REQUIRE(data.size() == 3);
  CHECK(data.samples()[0].value == doctest::Approx(1.0));
  CHECK(data.samples()[1].value == doctest::Approx(2.0));
  CHECK(data.samples()[2].value == doctest::Approx(3.0));
  CHECK(data.latest().value().value == doctest::Approx(3.0));
}

TEST_CASE("the capacity bound drops the oldest") {
  SeriesData data{3};
  for (int i = 1; i <= 5; ++i) {
    data.add({Timestamp{std::chrono::milliseconds{i * 100}}, static_cast<double>(i)});
  }

  CHECK(data.size() == 3);
  CHECK(data.samples().front().value == doctest::Approx(3.0));
  CHECK(data.samples().back().value == doctest::Approx(5.0));
}

TEST_CASE("a zero capacity is rejected") {
  CHECK_THROWS_AS(SeriesData{0}, std::invalid_argument);
  CHECK_NOTHROW(SeriesData{1});
}

TEST_CASE("a time range is half-open") {
  SeriesData data{10};
  for (int i = 1; i <= 5; ++i) {
    data.add({Timestamp{std::chrono::milliseconds{i * 100}}, static_cast<double>(i)});
  }

  const auto middle = data.between(Timestamp{std::chrono::milliseconds{200}},
                                   Timestamp{std::chrono::milliseconds{400}});
  REQUIRE(middle.size() == 2);
  CHECK(middle[0].value == doctest::Approx(2.0));
  CHECK(middle[1].value == doctest::Approx(3.0));

  CHECK(data.between(Timestamp{}, Timestamp{}).empty());
  CHECK(data.between(Timestamp{std::chrono::milliseconds{0}},
                     Timestamp{std::chrono::milliseconds{10000}})
            .size() == 5);
}

TEST_CASE("the store creates a series on first write, not on read") {
  MetricStore store{10};
  const SeriesKey cpu{"cpu.load", {{"host", "a"}}};

  CHECK(store.find(cpu) == nullptr);
  CHECK(store.series_count() == 0);
  CHECK_FALSE(store.latest(cpu).has_value());

  // Reading did not create it.
  CHECK(store.series_count() == 0);

  store.record(cpu, {Timestamp{std::chrono::milliseconds{100}}, 0.5});
  CHECK(store.series_count() == 1);
  CHECK(store.find(cpu) != nullptr);
  CHECK(store.latest(cpu).value().value == doctest::Approx(0.5));
}

TEST_CASE("tags make a different series") {
  MetricStore store;
  store.record({"cpu.load", {{"host", "a"}}},
               {Timestamp{std::chrono::milliseconds{1}}, 1.0});
  store.record({"cpu.load", {{"host", "b"}}},
               {Timestamp{std::chrono::milliseconds{1}}, 2.0});
  store.record({"cpu.load", {}}, {Timestamp{std::chrono::milliseconds{1}}, 3.0});

  CHECK(store.series_count() == 3);
  CHECK(store.sample_count() == 3);
}

TEST_CASE("keys come back in a deterministic order") {
  MetricStore store;
  store.record({"zeta", {}}, {Timestamp{}, 1.0});
  store.record({"alpha", {}}, {Timestamp{}, 1.0});
  store.record({"mu", {}}, {Timestamp{}, 1.0});

  const auto keys = store.keys();
  REQUIRE(keys.size() == 3);
  CHECK(keys[0].name == "alpha");
  CHECK(keys[1].name == "mu");
  CHECK(keys[2].name == "zeta");
}

TEST_CASE("dropping old data removes empty series with it") {
  MetricStore store;
  store.record({"old", {}}, {Timestamp{std::chrono::milliseconds{100}}, 1.0});
  store.record({"mixed", {}}, {Timestamp{std::chrono::milliseconds{100}}, 1.0});
  store.record({"mixed", {}}, {Timestamp{std::chrono::milliseconds{900}}, 2.0});

  CHECK(store.series_count() == 2);
  CHECK(store.sample_count() == 3);

  const std::size_t removed =
      store.drop_before(Timestamp{std::chrono::milliseconds{500}});

  CHECK(removed == 2);
  CHECK(store.sample_count() == 1);
  // "old" has nothing left, so it is gone entirely.
  CHECK(store.series_count() == 1);
  CHECK(store.keys()[0].name == "mixed");
}
