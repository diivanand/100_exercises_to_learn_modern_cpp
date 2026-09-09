// =============================================================================
//  06.08 -- std::tuple, and when a struct is better
// =============================================================================
//
//  `std::tuple<Ts...>` is a fixed-size collection of values of different
//  types. It is the type you reach for when you need to carry several values
//  around *generically* -- forwarding arguments, returning from a template,
//  building a compile-time list.
//
//      std::tuple<int, std::string, double> row{1, "ada", 2.5};
//      std::get<0>(row)                  by index
//      std::get<std::string>(row)        by type, if it appears exactly once
//      std::tuple_size_v<decltype(row)>  how many
//      std::apply(f, row)                call f with the elements as arguments
//      std::tie(a, b, c) = row           assign into existing variables
//
//  Structured bindings (01.06) make reading one pleasant, and are the reason
//  tuples became usable in ordinary code.
//
//  BUT: for a function's own return type, a named struct is almost always
//  better. `std::get<2>` says nothing; `result.error_count` says everything.
//  Use a tuple when the elements genuinely have no names -- generic code -- and
//  a struct when they do (Core Guidelines F.21).
//
//  One place a tuple is unbeatable: implementing comparison over several
//  members, because tuple's own comparison is lexicographic.
//
//  TASK
//    Implement the four functions below.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 06_08
//
// =============================================================================

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

struct Employee {
  std::string surname;
  std::string forename;
  int age = 0;
};

// Orders employees by surname, then forename, then age.
//
// TODO: implement with std::tie, which builds a tuple of REFERENCES (no
// copies) and compares them lexicographically. Writing the same ordering by
// hand takes six comparisons and is easy to get subtly wrong.
bool comes_before(const Employee& lhs, const Employee& rhs) {
  return false;
}

// Returns the smallest and largest values, and the count.
//
// TODO: return a std::tuple<int, int, std::size_t>. Then read the comment
// below about why this particular function should NOT return a tuple.
std::tuple<int, int, std::size_t> summarise(const std::vector<int>& values) {
  return {};
}

// Calls `f` with the elements of `arguments` spread out as parameters.
//
// TODO: one line, with std::apply. This is how you call a function with
// arguments you were handed as a pack -- the basis of std::thread, std::bind
// and every "invoke later" facility.
template <typename F, typename... Args>
auto call_with(F&& f, const std::tuple<Args...>& arguments) {
  return 0;
}

// Zips two vectors into pairs, stopping at the shorter one.
//
// TODO: implement it.
template <typename A, typename B>
std::vector<std::pair<A, B>> zip(const std::vector<A>& first,
                                 const std::vector<B>& second) {
  return {};
}

TEST_CASE("std::tie gives lexicographic ordering for free") {
  const Employee a{"Lovelace", "Ada", 36};
  const Employee b{"Lovelace", "Byron", 20};
  const Employee c{"Turing", "Alan", 41};

  CHECK(comes_before(a, b));
  CHECK(comes_before(b, c));
  CHECK_FALSE(comes_before(c, a));
  CHECK_FALSE(comes_before(a, a));

  std::vector<Employee> employees = {c, b, a};
  std::ranges::sort(employees, comes_before);
  CHECK(employees[0].forename == "Ada");
  CHECK(employees[2].surname == "Turing");
}

TEST_CASE("a tuple carries several values home") {
  const auto [smallest, largest, count] = summarise({3, 1, 4, 1, 5});
  CHECK(smallest == 1);
  CHECK(largest == 5);
  CHECK(count == 5);

  // ...and this is why a struct would be better here. Nothing in the type
  // `std::tuple<int, int, std::size_t>` tells a reader which int is which, and
  // swapping the first two is a silent bug. Compare with 02.06's MinMax.
  const auto empty = summarise({});
  CHECK(std::get<2>(empty) == 0);
}

TEST_CASE("std::apply spreads a tuple into arguments") {
  const auto add = [](int a, int b, int c) { return a + b + c; };
  CHECK(call_with(add, std::tuple{1, 2, 3}) == 6);

  const auto join = [](const std::string& a, const std::string& b) { return a + b; };
  CHECK(call_with(join, std::tuple{std::string{"mod"}, std::string{"ern"}}) == "modern");
}

TEST_CASE("zip stops at the shorter input") {
  const std::vector<int> numbers = {1, 2, 3};
  const std::vector<std::string> names = {"one", "two"};

  const auto zipped = zip(numbers, names);
  REQUIRE(zipped.size() == 2);
  CHECK(zipped[0].first == 1);
  CHECK(zipped[0].second == "one");
  CHECK(zipped[1].first == 2);

  CHECK(zip(std::vector<int>{}, names).empty());
}
