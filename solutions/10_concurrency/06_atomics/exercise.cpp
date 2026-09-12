// Solution -- 10.06 std::atomic
#include <doctest/doctest.h>

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

int add_and_get_old(std::atomic<int>& counter, int amount) {
  return counter.fetch_add(amount);
}

void store_if_greater(std::atomic<int>& high_water, int amount) {
  int current = high_water.load();
  // compare_exchange_weak writes the actual value back into `current` when it
  // fails, so the loop condition is re-evaluated against fresh data with no
  // extra load.
  while (current < amount && !high_water.compare_exchange_weak(current, amount)) {
  }
}

void multiply(std::atomic<int>& value, int factor) {
  int current = value.load();
  while (!value.compare_exchange_weak(current, current * factor)) {
  }
}

class Stack {
public:
  void push(int value) {
    Node* node = new Node{value, head_.load()};
    // If another thread pushed in between, `node->next` is updated by the
    // failed compare_exchange and we try again with the new head.
    while (!head_.compare_exchange_weak(node->next, node)) {
    }
  }

  int depth() const {
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

  long total() const {
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
