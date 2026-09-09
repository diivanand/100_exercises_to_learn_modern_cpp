// =============================================================================
//  07.02 -- Projections
// =============================================================================
//
//  Half the lambdas people write for algorithms do nothing but reach into a
//  member:
//
//      std::sort(people.begin(), people.end(),
//                [](const Person& a, const Person& b) { return a.age < b.age; });
//
//  Every C++20 ranges algorithm takes an optional PROJECTION as its last
//  argument: a function applied to each element before the algorithm looks at
//  it. The comparison, the predicate and the search value all then operate on
//  the projected value:
//
//      std::ranges::sort(people, {}, &Person::age);
//                                ^^  ^^^^^^^^^^^^
//                                |   projection: what to look at
//                                default comparison: std::ranges::less
//
//  A projection can be a pointer to member (data or function), a lambda, or
//  anything invocable. `std::invoke` semantics mean `&Person::name` works for
//  both a member variable and a member function.
//
//  This is not only shorter. `ranges::find(people, "ada", &Person::name)` says
//  "find the person whose name is ada"; the lambda version says "find the
//  person for which this returns true", and you have to read the lambda.
//
//  TASK
//    Rewrite each call with a projection. Two of the lambdas below are wrong
//    in a way that is easy to miss and hard to write once the projection is
//    doing the reaching for you.
//
//  RUN IT
//    ./mcpp test 07_02
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
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

// TODO: `std::ranges::sort(people, {}, &Person::age)`.
void sort_by_age(std::vector<Person>& people) {
  std::ranges::sort(people,
                    [](const Person& a, const Person& b) { return a.age < b.age; });
}

// TODO: sort by name descending. The comparator slot takes
// `std::ranges::greater{}`, and the projection slot takes the member.
void sort_by_name_descending(std::vector<Person>& people) {
  std::ranges::sort(people,
                    [](const Person& a, const Person& b) { return a.name < b.name; });
}

// TODO: `std::ranges::find(people, name, &Person::name)`.
std::optional<Person> find_by_name(const std::vector<Person>& people,
                                   const std::string& name) {
  const auto it = std::ranges::find_if(
      people, [&name](const Person& person) { return person.name == name; });
  return it == people.end() ? std::nullopt : std::optional{*it};
}

// TODO: `std::ranges::max_element` with a projection. Note that the iterator
// still points at the whole Person -- the projection only affects how the
// algorithm compares.
std::optional<Person> tallest(const std::vector<Person>& people) {
  if (people.empty()) {
    return std::nullopt;
  }
  const auto it = std::ranges::max_element(
      people, [](const Person& a, const Person& b) { return a.height < b.height; });
  return *it;
}

// TODO: a projection can be a member FUNCTION too. Count the people whose
// initials are "A", projecting through &Person::initials.
int count_with_initial(const std::vector<Person>& people, const std::string& initial) {
  return static_cast<int>(std::ranges::count_if(
      people, [&initial](const Person& person) { return person.name == initial; }));
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
