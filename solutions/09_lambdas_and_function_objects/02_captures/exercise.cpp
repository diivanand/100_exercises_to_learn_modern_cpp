// Solution -- 09.02 Captures
#include <doctest/doctest.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

std::function<std::string(const std::string&)> make_greeter(std::string greeting) {
  // An init capture that MOVES the parameter into the lambda: the lambda owns
  // it, and no copy is made on the way in.
  return [greeting = std::move(greeting)](const std::string& name) {
    return greeting + ", " + name + "!";
  };
}

std::function<int()> make_counter() {
  // `mutable` makes operator() non-const, so the captured count can change.
  return [count = 0]() mutable { return count++; };
}

int sum_and_count_odd(const std::vector<int>& values, int& odd_count) {
  int total = 0;
  // Both accumulators by reference, both named: the capture list is now a
  // list of exactly what this lambda shares with its enclosing scope.
  const auto visit = [&total, &odd_count](int value) {
    total += value;
    if (value % 2 != 0) {
      odd_count += 1;
    }
  };
  for (const int value : values) {
    visit(value);
  }
  return total;
}

class Accumulator {
public:
  explicit Accumulator(int start) : total_(start) {}

  [[nodiscard]] std::function<int(int)> adder() const {
    // Captures the value it needs, not the object it came from. The result is
    // independent of this Accumulator's lifetime.
    return [total = total_](int value) { return total + value; };
  }

  [[nodiscard]] int total() const noexcept {
    return total_;
  }

private:
  int total_;
};

TEST_CASE("a lambda that escapes must own what it captured") {
  const auto greet = make_greeter("Hello");
  CHECK(greet("world") == "Hello, world!");
  CHECK(greet("again") == "Hello, again!");
}

TEST_CASE("mutable lets a lambda carry state") {
  auto counter = make_counter();
  CHECK(counter() == 0);
  CHECK(counter() == 1);
  CHECK(counter() == 2);

  auto other = make_counter();
  CHECK(other() == 0);
  CHECK(counter() == 3);
}

TEST_CASE("capture the accumulator by reference, by name") {
  int odd_count = 0;
  CHECK(sum_and_count_odd({1, 2, 3, 4, 5}, odd_count) == 15);
  CHECK(odd_count == 3);

  odd_count = 0;
  CHECK(sum_and_count_odd({2, 4}, odd_count) == 6);
  CHECK(odd_count == 0);
}

TEST_CASE("a callable that outlives its object must not capture this") {
  std::function<int(int)> add;
  {
    const Accumulator accumulator{100};
    add = accumulator.adder();
    CHECK(add(5) == 105);
  }
  CHECK(add(5) == 105);
}
