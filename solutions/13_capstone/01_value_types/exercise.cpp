// Solution -- 13.01 Value types for the metrics store
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

// A strongly typed name. Preventing a metric name and a tag value from being
// interchangeable is worth one small class (04.02).
class MetricName {
public:
  explicit MetricName(std::string value) : value_(std::move(value)) {
    if (value_.empty()) {
      throw std::invalid_argument{"metric name must not be empty"};
    }
  }

  const std::string& value() const noexcept {
    return value_;
  }

  friend bool operator==(const MetricName&, const MetricName&) = default;
  friend std::strong_ordering operator<=>(const MetricName&, const MetricName&) = default;

private:
  std::string value_;
};

// A timestamp. system_clock because these are wall-clock instants that get
// serialised, not durations being measured (12.02).
using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;

// One observation. An aggregate with defaults, so designated initialisers work
// and a partially specified sample is still valid (02.05).
struct Sample {
  Timestamp at{};
  double value = 0.0;

  friend bool operator==(const Sample&, const Sample&) = default;
  // Ordered by time, then by value -- declaration order is the ordering, which
  // is exactly what a defaulted <=> gives (04.08).
  friend std::partial_ordering operator<=>(const Sample&, const Sample&) = default;
};

// A set of key/value tags. Rule of zero (03.04): the members manage
// themselves, so the compiler writes all five special members correctly.
class Tags {
public:
  Tags() = default;
  Tags(std::initializer_list<std::pair<const std::string, std::string>> entries)
      : entries_(entries) {}

  Tags& set(std::string key, std::string value) {
    entries_.insert_or_assign(std::move(key), std::move(value));
    return *this;
  }

  std::string_view get(const std::string& key) const {
    const auto it = entries_.find(key);
    return it == entries_.end() ? std::string_view{} : std::string_view{it->second};
  }

  bool contains(const std::string& key) const {
    return entries_.contains(key);
  }

  std::size_t size() const noexcept {
    return entries_.size();
  }
  [[nodiscard]] bool empty() const noexcept {
    return entries_.empty();
  }

  auto begin() const {
    return entries_.begin();
  }
  auto end() const {
    return entries_.end();
  }

  friend bool operator==(const Tags&, const Tags&) = default;

private:
  std::map<std::string, std::string> entries_;
};

// A named, tagged series of one metric.
struct Series {
  MetricName name;
  Tags tags;

  friend bool operator==(const Series&, const Series&) = default;
};

template <>
struct std::formatter<MetricName> : std::formatter<std::string> {
  auto format(const MetricName& name, std::format_context& context) const {
    return std::formatter<std::string>::format(name.value(), context);
  }
};

template <>
struct std::formatter<Series> : std::formatter<std::string> {
  auto format(const Series& series, std::format_context& context) const {
    std::string result = series.name.value();
    if (!series.tags.empty()) {
      result += '{';
      for (bool first = true; const auto& [key, value] : series.tags) {
        if (!first) {
          result += ',';
        }
        result += std::format("{}={}", key, value);
        first = false;
      }
      result += '}';
    }
    return std::formatter<std::string>::format(result, context);
  }
};

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
