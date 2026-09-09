// =============================================================================
//  02.06 -- Choosing a parameter type
// =============================================================================
//
//  There is a decision procedure for this, and it is short.
//
//    READ-ONLY, cheap to copy (int, double, a pointer, string_view, span):
//        pass by value.                                    T
//
//    READ-ONLY, expensive to copy (containers, big structs):
//        pass by reference to const.                       const T&
//
//    THE FUNCTION WILL KEEP A COPY (a "sink"):
//        pass by value and std::move it into place.        T
//        The caller decides: an lvalue is copied once, an rvalue is moved.
//
//    THE FUNCTION MODIFIES THE CALLER'S OBJECT:
//        pass by reference.                                T&
//        (Prefer returning a new value where you can -- see below.)
//
//  Two corollaries that matter more than they look:
//
//   * "Cheap to copy" means roughly "fits in a couple of registers and does
//     not allocate". `std::string_view` and `std::span` exist so that reading
//     a sequence never needs a reference at all (Core Guidelines F.16).
//
//   * Prefer returning a value to filling in an out-parameter (F.20). Return
//     values compose, can be `const`, cannot be forgotten, and since C++17
//     are not copied on the way out.
//
//  TASK
//    Fix the four signatures below. The bodies are already right.
//
//
//  NOTE  This exercise starts as a compile error: the tests call the
//        signatures you are being asked to write, not the ones that are there.
//
//  RUN IT
//    ./mcpp test 02_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

// TODO: this copies the whole vector on every call, to read it.
int sum(std::vector<int> values) {
  int total = 0;
  for (const int value : values) {
    total += value;
  }
  return total;
}

// TODO: `text` is only read, and the function never needs to own it. A
// `const std::string&` forces every caller holding a literal or a
// string_view to build a std::string first.
std::size_t count_vowels(const std::string& text) {
  std::size_t count = 0;
  for (const char c : text) {
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
      ++count;
    }
  }
  return count;
}

class Logger {
public:
  // TODO: this is a sink -- the prefix is stored. Taking `const std::string&`
  // means a caller passing a temporary still pays for a copy. Take it by
  // value and move it into the member.
  explicit Logger(const std::string& prefix) : prefix_(prefix) {}

  [[nodiscard]] std::string format(std::string_view message) const {
    return prefix_ + ": " + std::string{message};
  }

private:
  std::string prefix_;
};

// TODO: an out-parameter for something the function could simply return.
// Change it to return the result. (Both values are needed, so return a small
// struct -- naming the fields beats returning a std::pair.)
struct MinMax {
  int min = 0;
  int max = 0;
};

void min_max(const std::vector<int>& values, MinMax& out) {
  if (values.empty()) {
    out = MinMax{};
    return;
  }
  out.min = values.front();
  out.max = values.front();
  for (const int value : values) {
    if (value < out.min) {
      out.min = value;
    }
    if (value > out.max) {
      out.max = value;
    }
  }
}

TEST_CASE("sum reads without copying") {
  const std::vector<int> values = {1, 2, 3};
  CHECK(sum(values) == 6);
}

TEST_CASE("count_vowels accepts anything string-like") {
  CHECK(count_vowels("hello") == 2);
  CHECK(count_vowels(std::string{"queueing"}) == 5);

  // This is the line that fails with a `const std::string&` parameter unless
  // the caller builds a temporary:
  constexpr std::string_view view = "modern";
  CHECK(count_vowels(view) == 2);
}

TEST_CASE("Logger takes ownership of its prefix") {
  Logger logger{"app"};
  CHECK(logger.format("started") == "app: started");

  // Moving a temporary into the constructor should not allocate a second
  // string. There is no portable way to assert that, so this is a reminder
  // rather than a check.
  std::string prefix = "service";
  Logger moved{std::move(prefix)};
  CHECK(moved.format("ok") == "service: ok");
}

TEST_CASE("min_max returns its result") {
  const MinMax result = min_max({3, 1, 4, 1, 5});
  CHECK(result.min == 1);
  CHECK(result.max == 5);

  const MinMax empty = min_max({});
  CHECK(empty.min == 0);
  CHECK(empty.max == 0);
}
