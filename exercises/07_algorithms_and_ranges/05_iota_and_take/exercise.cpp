// =============================================================================
//  07.05 -- Generating ranges: iota, take, drop, take_while
// =============================================================================
//
//  Not every range comes from a container. `std::views::iota` generates one:
//
//      std::views::iota(0, 5)    // 0 1 2 3 4  -- bounded
//      std::views::iota(0)       // 0 1 2 3 ... -- unbounded
//
//  An unbounded range is only useful because everything downstream is lazy
//  (07.04). `iota(1) | filter(is_prime) | take(10)` computes exactly ten
//  primes; nothing tries to build the infinite list.
//
//  The adaptors that shape a range:
//
//      take(n)            at most the first n
//      drop(n)            everything after the first n
//      take_while(pred)   the leading run for which pred holds -- STOPS at the
//                         first failure, unlike filter which keeps looking
//      drop_while(pred)   skip the leading run, keep the rest
//      reverse            backwards (needs a bidirectional range)
//      elements<N>        the Nth element of each tuple-like element
//      keys / values      elements<0> / elements<1>, for maps
//
//  `take_while` versus `filter` is the distinction worth internalising:
//  filter(is_even) over 1,2,3,4 gives 2,4; take_while(is_even) gives nothing,
//  because the very first element failed.
//
//  TASK
//    Build the generators the tests describe.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 07_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <map>
#include <ranges>
#include <string>
#include <vector>

// TODO: return a view of the integers `first`..`last-1`.
auto range_of(int first, int last) {
  return std::views::iota(0, 0);
}

// TODO: return a view of the first `count` multiples of `step`, starting at
// `step`. Build it from an unbounded iota so that nothing is computed until
// it is asked for.
auto multiples(int step, int count) {
  return std::views::iota(0, 0);
}

// Returns the leading run of values below `limit`, stopping at the first one
// that is not.
//
// TODO: take_while, not filter.
auto leading_below(const std::vector<int>& values, int limit) {
  return values;
}

// TODO: return a view of a map's keys. `std::views::keys` is
// `elements<0>` with a better name.
auto keys_of(const std::map<std::string, int>& table) {
  return table;
}

// Returns the last `count` elements.
//
// TODO: `drop` skips from the front, so you need to drop `size - count` --
// taking care that the subtraction does not wrap when count > size.
auto last_n(const std::vector<int>& values, std::size_t count) {
  return values;
}

namespace {

template <std::ranges::range R>
std::vector<std::ranges::range_value_t<R>> collect(R&& range) {
  std::vector<std::ranges::range_value_t<R>> result;
  for (auto&& element : range) {
    result.push_back(element);
  }
  return result;
}

} // namespace

TEST_CASE("iota generates a bounded range") {
  CHECK(collect(range_of(0, 5)) == std::vector<int>{0, 1, 2, 3, 4});
  CHECK(collect(range_of(3, 6)) == std::vector<int>{3, 4, 5});
  CHECK(collect(range_of(0, 0)).empty());
}

TEST_CASE("an unbounded range is fine when something downstream stops it") {
  CHECK(collect(multiples(3, 4)) == std::vector<int>{3, 6, 9, 12});
  CHECK(collect(multiples(10, 1)) == std::vector<int>{10});
  CHECK(collect(multiples(2, 0)).empty());
}

TEST_CASE("take_while stops at the first failure; filter would not") {
  const std::vector<int> values = {1, 2, 9, 3, 4};

  CHECK(collect(leading_below(values, 5)) == std::vector<int>{1, 2});
  CHECK(collect(leading_below(values, 100)) == std::vector<int>{1, 2, 9, 3, 4});
  CHECK(collect(leading_below(values, 0)).empty());
}

TEST_CASE("keys of a map") {
  const std::map<std::string, int> table = {{"a", 1}, {"b", 2}, {"c", 3}};
  CHECK(collect(keys_of(table)) == std::vector<std::string>{"a", "b", "c"});
}

TEST_CASE("last_n") {
  const std::vector<int> values = {1, 2, 3, 4, 5};
  CHECK(collect(last_n(values, 2)) == std::vector<int>{4, 5});
  CHECK(collect(last_n(values, 5)) == std::vector<int>{1, 2, 3, 4, 5});
  // More than there are: everything, not a wrapped-around disaster.
  CHECK(collect(last_n(values, 99)) == std::vector<int>{1, 2, 3, 4, 5});
  CHECK(collect(last_n(values, 0)).empty());
}
