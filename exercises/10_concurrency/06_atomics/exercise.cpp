// =============================================================================
//  10.06 -- std::atomic
// =============================================================================
//
//  An atomic operation is indivisible: no other thread can observe it half
//  done. `std::atomic<int>` gives you that for a single value, usually without
//  a lock -- `is_lock_free()` says whether, and `is_always_lock_free` is the
//  compile-time version.
//
//  The operations:
//
//      load() / store()             read / write
//      exchange(v)                  set, and return the old value
//      fetch_add / fetch_sub        arithmetic, returning the OLD value
//      fetch_and / fetch_or / fetch_xor
//      compare_exchange_weak/strong the one that makes lock-free algorithms
//                                   possible
//
//  COMPARE-AND-SWAP is the primitive everything else is built on:
//
//      bool compare_exchange_strong(T& expected, T desired);
//
//  "If the value is `expected`, set it to `desired` and return true.
//   Otherwise write the actual value into `expected` and return false."
//
//  That is why the retry loop is written the way it is: `expected` is updated
//  for you, so the loop body just recomputes and tries again.
//
//      T current = value.load();
//      while (!value.compare_exchange_weak(current, f(current))) { }
//
//  `_weak` may fail spuriously and is cheaper on some architectures -- use it
//  inside a loop that would retry anyway; use `_strong` when there is no loop.
//
//  WHAT ATOMICS ARE NOT: a substitute for a mutex when several values must
//  change together (10.02). Two atomics are not one atomic.
//
//  TASK
//    Implement the four operations.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 10_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

// TODO: add `amount` and return the value BEFORE the addition. One call.
int add_and_get_old(std::atomic<int>& counter, int amount) {
  return 0;
}

// TODO: set the value to `amount` only if the current value is smaller.
// This needs a compare-exchange loop: read, decide, try to swap, retry if
// somebody beat you to it.
void store_if_greater(std::atomic<int>& high_water, int amount) {}

// TODO: multiply the value by `factor`, atomically. There is no fetch_mul, so
// this is a compare-exchange loop too -- which is the general recipe for
// "atomically apply any function".
void multiply(std::atomic<int>& value, int factor) {}

// A lock-free stack of integers, built on compare_exchange.
//
// TODO: implement push. Read the head, point the new node at it, and swap the
// new node in -- retrying if another thread changed the head in between.
//
// (Popping safely is much harder -- it needs hazard pointers or reference
// counting to avoid freeing a node another thread is still reading. That is
// why "just use a mutex" is usually the right answer.)
class Stack {
public:
  void push(int value) {}

  [[nodiscard]] int depth() const {
    int count = 0;
    for (const Node* node = head_.load(); node != nullptr; node = node->next) {
      ++count;
    }
    return count;
  }

  ~Stack() {
    const Node* node = head_.load();
    while (node != nullptr) {
      const Node* next = node->next;
      delete node;
      node = next;
    }
  }

  Stack() = default;
  Stack(const Stack&) = delete;
  Stack& operator=(const Stack&) = delete;

  [[nodiscard]] long total() const {
    long sum = 0;
    for (const Node* node = head_.load(); node != nullptr; node = node->next) {
      sum += node->value;
    }
    return sum;
  }

private:
  struct Node {
    int value = 0;
    Node* next = nullptr;
  };

  std::atomic<Node*> head_{nullptr};
};

TEST_CASE("fetch_add returns the old value") {
  std::atomic<int> counter{10};
  CHECK(add_and_get_old(counter, 5) == 10);
  CHECK(counter.load() == 15);
  CHECK(add_and_get_old(counter, -15) == 15);
  CHECK(counter.load() == 0);
}

TEST_CASE("a high-water mark, from many threads") {
  std::atomic<int> high_water{0};

  {
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<std::size_t>(8));
    for (int t = 0; t < 8; ++t) {
      workers.emplace_back([&high_water, t] {
        for (int i = 0; i <= 100; ++i) {
          store_if_greater(high_water, t * 100 + i);
        }
      });
    }
  }

  CHECK(high_water.load() == 7 * 100 + 100);
}

TEST_CASE("applying an arbitrary function atomically") {
  std::atomic<int> value{1};

  {
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<std::size_t>(10));
    for (int t = 0; t < 10; ++t) {
      workers.emplace_back([&value] { multiply(value, 2); });
    }
  }

  // 2^10. Every multiplication happened exactly once.
  CHECK(value.load() == 1024);
}

TEST_CASE("a lock-free push") {
  Stack stack;
  {
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<std::size_t>(4));
    for (int t = 0; t < 4; ++t) {
      workers.emplace_back([&stack] {
        for (int i = 1; i <= 100; ++i) {
          stack.push(i);
        }
      });
    }
  }

  CHECK(stack.depth() == 400);
  CHECK(stack.total() == 4 * 100 * 101 / 2);
}

TEST_CASE("atomics on a normal machine are lock-free") {
  static_assert(std::atomic<int>::is_always_lock_free);
  static_assert(std::atomic<void*>::is_always_lock_free);
  CHECK(true);
}
