// Solution -- 08.02 Class templates and CTAD
#include <doctest/doctest.h>

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T>
class Stack {
public:
  Stack() = default;

  Stack(std::initializer_list<T> items) : items_(items) {}

  template <typename It>
  Stack(It first, It last) : items_(first, last) {}

  void push(T value) {
    items_.push_back(std::move(value));
  }

  T pop() {
    if (items_.empty()) {
      throw std::out_of_range{"pop from an empty stack"};
    }
    T value = std::move(items_.back());
    items_.pop_back();
    return value;
  }

  [[nodiscard]] const T& top() const {
    if (items_.empty()) {
      throw std::out_of_range{"top of an empty stack"};
    }
    return items_.back();
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return items_.size();
  }
  [[nodiscard]] bool empty() const noexcept {
    return items_.empty();
  }

private:
  std::vector<T> items_;
};

// The initializer_list constructor already gives the compiler enough to deduce
// T, so this guide is redundant -- but writing it out costs nothing and
// documents the intent.
template <typename T>
Stack(std::initializer_list<T>) -> Stack<T>;

// This one is necessary: nothing in `Stack(It, It)` relates It to T.
template <typename It>
Stack(It, It) -> Stack<typename std::iterator_traits<It>::value_type>;

TEST_CASE("CTAD from an initializer list") {
  Stack stack{1, 2, 3};
  static_assert(std::is_same_v<decltype(stack), Stack<int>>);

  CHECK(stack.size() == 3);
  CHECK(stack.top() == 3);
  CHECK(stack.pop() == 3);
  CHECK(stack.size() == 2);
}

TEST_CASE("CTAD works for any element type") {
  Stack names{std::string{"ada"}, std::string{"alan"}};
  static_assert(std::is_same_v<decltype(names), Stack<std::string>>);
  CHECK(names.top() == "alan");
}

TEST_CASE("CTAD from an iterator pair needs a deduction guide") {
  const std::vector<double> values = {1.5, 2.5, 3.5};
  // Parentheses, not braces. With braces this would be a two-element
  // initializer_list OF ITERATORS -- 01.02's trap, in a new costume: an
  // initializer_list constructor wins over everything else in list
  // initialisation, deduction guides included.
  Stack stack(values.begin(), values.end());

  static_assert(std::is_same_v<decltype(stack), Stack<double>>);
  CHECK(stack.size() == 3);
  CHECK(stack.top() == doctest::Approx(3.5));
}

TEST_CASE("the explicit form still works, and is sometimes clearer") {
  Stack<int> stack;
  CHECK(stack.empty());
  stack.push(1);
  CHECK(stack.top() == 1);

  CHECK_THROWS_AS((void)Stack<int>{}.pop(), std::out_of_range);
}
