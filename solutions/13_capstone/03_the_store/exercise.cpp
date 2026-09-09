// Solution -- 13.03 Capstone: the store
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
  explicit SeriesData(std::size_t capacity) : capacity_(capacity) {
    if (capacity == 0) {
      throw std::invalid_argument{"capacity must be at least 1"};
    }
  }

  // Out-of-order arrival is normal for metrics, so this inserts in the right
  // place rather than assuming append.
  void add(Sample sample) {
    const auto position = std::ranges::upper_bound(samples_, sample.at, {}, &Sample::at);
    samples_.insert(position, sample);

    while (samples_.size() > capacity_) {
      samples_.erase(samples_.begin());
    }
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return samples_.size();
  }
  [[nodiscard]] bool empty() const noexcept {
    return samples_.empty();
  }
  [[nodiscard]] std::size_t capacity() const noexcept {
    return capacity_;
  }

  // A read-only view of the samples. Returning a const reference rather than a
  // copy, and const so the invariant cannot be broken through it.
  [[nodiscard]] const std::vector<Sample>& samples() const noexcept {
    return samples_;
  }

  [[nodiscard]] std::optional<Sample> latest() const {
    if (samples_.empty()) {
      return std::nullopt;
    }
    return samples_.back();
  }

  // The half-open range [from, to). Binary search, because the samples are
  // sorted -- which is the point of maintaining the invariant.
  [[nodiscard]] std::vector<Sample> between(Timestamp from, Timestamp to) const {
    const auto first = std::ranges::lower_bound(samples_, from, {}, &Sample::at);
    const auto last = std::ranges::lower_bound(samples_, to, {}, &Sample::at);
    return {first, last};
  }

  // Removes everything before `cutoff`, returning how many went.
  std::size_t drop_before(Timestamp cutoff) {
    const auto first_kept = std::ranges::lower_bound(samples_, cutoff, {}, &Sample::at);
    const auto removed = static_cast<std::size_t>(first_kept - samples_.begin());
    samples_.erase(samples_.begin(), first_kept);
    return removed;
  }

private:
  std::size_t capacity_;
  std::vector<Sample> samples_;
};

class MetricStore {
public:
  explicit MetricStore(std::size_t capacity_per_series = 1000)
      : capacity_per_series_(capacity_per_series) {}

  void record(SeriesKey key, Sample sample) {
    // try_emplace constructs the SeriesData only when the key is new (06.07).
    const auto [position, inserted] =
        series_.try_emplace(std::move(key), capacity_per_series_);
    position->second.add(sample);
  }

  [[nodiscard]] std::size_t series_count() const noexcept {
    return series_.size();
  }

  [[nodiscard]] std::size_t sample_count() const {
    std::size_t total = 0;
    for (const auto& [key, data] : series_) {
      total += data.size();
    }
    return total;
  }

  // Reading a missing key must not create it, so `find`, not operator[]
  // (06.06). Returns a pointer rather than a reference so absence is
  // expressible without an exception.
  [[nodiscard]] const SeriesData* find(const SeriesKey& key) const {
    const auto it = series_.find(key);
    return it == series_.end() ? nullptr : &it->second;
  }

  [[nodiscard]] std::optional<Sample> latest(const SeriesKey& key) const {
    const SeriesData* data = find(key);
    return data == nullptr ? std::nullopt : data->latest();
  }

  [[nodiscard]] std::vector<SeriesKey> keys() const {
    std::vector<SeriesKey> result;
    result.reserve(series_.size());
    for (const auto& [key, data] : series_) {
      result.push_back(key);
    }
    return result;
  }

  std::size_t drop_before(Timestamp cutoff) {
    std::size_t removed = 0;
    for (auto& [key, data] : series_) {
      removed += data.drop_before(cutoff);
    }
    // A series with nothing left is dropped too -- C++20's erase_if (06.09).
    std::erase_if(series_, [](const auto& entry) { return entry.second.empty(); });
    return removed;
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
