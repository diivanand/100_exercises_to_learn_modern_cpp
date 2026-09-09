// =============================================================================
//  08.02 -- Class templates and CTAD
// =============================================================================
//
//  Before C++17, a class template's arguments had to be spelled out, even when
//  they were obvious -- which is why `std::make_pair` and `std::make_tuple`
//  existed at all:
//
//      std::pair<int, std::string> p{1, "one"};
//      auto p = std::make_pair(1, std::string{"one"});   // the workaround
//
//  CLASS TEMPLATE ARGUMENT DEDUCTION deduces them from the constructor:
//
//      std::pair p{1, std::string{"one"}};   // std::pair<int, std::string>
//      std::vector v{1, 2, 3};               // std::vector<int>
//      std::lock_guard guard{mutex};         // std::lock_guard<std::mutex>
//
//  The compiler forms implicit deduction guides from the constructors. When
//  those are not enough -- because a constructor takes an iterator pair, say,
//  and the element type is `iterator_traits<It>::value_type` -- you write an
//  explicit DEDUCTION GUIDE:
//
//      template <typename It>
//      Vector(It, It) -> Vector<typename std::iterator_traits<It>::value_type>;
//
//  You have already used one: `ScopeExit{[]{...}}` in 05.03 works because the
//  implicit guide deduces the lambda's type.
//
//  TASK
//    Finish `Stack`, then give it the deduction guides the tests need.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 08_02
//
// =============================================================================

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

  // TODO: a constructor from an initializer_list, so `Stack{1, 2, 3}` works.
  // TODO: a constructor from an iterator pair.

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

// TODO: deduction guides.
//
//   * From an initializer_list<T> -> Stack<T>. (The implicit guide from the
//     constructor may already handle this one -- check whether you need it.)
//   * From a pair of iterators -> Stack<iterator_traits<It>::value_type>.
//     This one the compiler cannot work out on its own: nothing relates `It`
//     to `T` without your saying so.

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
