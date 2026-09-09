// Solution -- 01.06 Structured bindings
#include <doctest/doctest.h>

#include <map>
#include <set>
#include <string>
#include <vector>

struct Measurement {
  std::string sensor;
  double value = 0.0;
  bool valid = false;
};

int total_score(const std::map<std::string, int>& scores) {
  int total = 0;
  for (const auto& [name, score] : scores) {
    total += static_cast<int>(name.size()) * score;
  }
  return total;
}

bool remember(std::set<std::string>& seen, const std::string& name) {
  const auto [position, inserted] = seen.insert(name);
  return inserted;
}

void scale_valid(std::vector<Measurement>& measurements, double factor) {
  for (auto& [sensor, value, valid] : measurements) {
    if (valid) {
      value *= factor;
    }
  }
}

TEST_CASE("total_score sums the values") {
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
