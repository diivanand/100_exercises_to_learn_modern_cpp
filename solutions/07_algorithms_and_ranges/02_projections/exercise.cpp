// Solution -- 07.02 Projections
#include <doctest/doctest.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct Person {
  std::string name;
  int age = 0;
  double height = 0.0;

  [[nodiscard]] std::string initials() const {
    return name.empty() ? "" : std::string(1, name.front());
  }
};

void sort_by_age(std::vector<Person>& people) {
  // {} means "the default comparison", std::ranges::less.
  std::ranges::sort(people, {}, &Person::age);
}

void sort_by_name_descending(std::vector<Person>& people) {
  std::ranges::sort(people, std::ranges::greater{}, &Person::name);
}

std::optional<Person> find_by_name(const std::vector<Person>& people,
                                   const std::string& name) {
  const auto it = std::ranges::find(people, name, &Person::name);
  return it == people.end() ? std::nullopt : std::optional{*it};
}

std::optional<Person> tallest(const std::vector<Person>& people) {
  if (people.empty()) {
    return std::nullopt;
  }
  // The projection decides what is compared; the iterator still refers to the
  // whole element.
  const auto it = std::ranges::max_element(people, {}, &Person::height);
  return *it;
}

int count_with_initial(const std::vector<Person>& people, const std::string& initial) {
  return static_cast<int>(std::ranges::count(people, initial, &Person::initials));
}

namespace {

std::vector<Person> sample() {
  return {{"ada", 36, 1.70}, {"alan", 41, 1.80}, {"grace", 45, 1.65}};
}

} // namespace

TEST_CASE("sort by age") {
  auto people = sample();
  sort_by_age(people);
  CHECK(people[0].name == "ada");
  CHECK(people[2].name == "grace");
}

TEST_CASE("sort by name, descending") {
  auto people = sample();
  sort_by_name_descending(people);
  CHECK(people[0].name == "grace");
  CHECK(people[1].name == "alan");
  CHECK(people[2].name == "ada");
}

TEST_CASE("find by name") {
  const auto people = sample();
  const auto found = find_by_name(people, "alan");
  REQUIRE(found.has_value());
  CHECK(found->age == 41);

  CHECK_FALSE(find_by_name(people, "nobody").has_value());
}

TEST_CASE("tallest") {
  const auto people = sample();
  const auto found = tallest(people);
  REQUIRE(found.has_value());
  CHECK(found->name == "alan");

  CHECK_FALSE(tallest({}).has_value());
}

TEST_CASE("projecting through a member function") {
  const auto people = sample();
  CHECK(count_with_initial(people, "a") == 2);
  CHECK(count_with_initial(people, "g") == 1);
  CHECK(count_with_initial(people, "z") == 0);
}
