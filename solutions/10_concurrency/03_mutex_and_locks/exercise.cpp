// Solution -- 10.03 Mutexes, lock guards, and deadlock
#include <doctest/doctest.h>

#include <atomic>
#include <cstddef>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class Queue {
public:
  void push(int value) {
    const std::lock_guard lock{mutex_};
    if (value < 0) {
      // The lock_guard's destructor runs as this unwinds, so the mutex is
      // released. That is the whole argument for RAII locking.
      throw std::invalid_argument{"negative"};
    }
    items_.push_back(value);
  }

  [[nodiscard]] std::size_t size() {
    const std::lock_guard lock{mutex_};
    return items_.size();
  }

private:
  std::mutex mutex_;
  std::vector<int> items_;
};

struct Account {
  std::mutex mutex;
  int balance = 0;
};

void transfer(Account& from, Account& to, int amount) {
  // scoped_lock takes both mutexes with a deadlock-avoiding algorithm, so the
  // order the arguments appear in does not matter.
  const std::scoped_lock lock{from.mutex, to.mutex};
  from.balance -= amount;
  to.balance += amount;
}

class Cache {
public:
  void put(std::string key, int value) {
    // A writer needs exclusive access.
    const std::unique_lock lock{mutex_};
    entries_.insert_or_assign(std::move(key), value);
  }

  [[nodiscard]] int get(const std::string& key) const {
    // Readers share: any number of these can run at once.
    const std::shared_lock lock{mutex_};
    const auto it = entries_.find(key);
    return it == entries_.end() ? -1 : it->second;
  }

  [[nodiscard]] std::size_t size() const {
    const std::shared_lock lock{mutex_};
    return entries_.size();
  }

private:
  mutable std::shared_mutex mutex_;
  std::map<std::string, int> entries_;
};

TEST_CASE("a throw must not leave the mutex locked") {
  Queue queue;
  queue.push(1);
  CHECK_THROWS_AS(queue.push(-1), std::invalid_argument);

  queue.push(2);
  CHECK(queue.size() == 2);
}

TEST_CASE("transfers in both directions at once must not deadlock") {
  Account a{{}, 1000};
  Account b{{}, 1000};

  {
    std::vector<std::jthread> workers;
    workers.emplace_back([&a, &b] {
      for (int i = 0; i < 2000; ++i) {
        transfer(a, b, 1);
      }
    });
    workers.emplace_back([&b, &a] {
      for (int i = 0; i < 2000; ++i) {
        transfer(b, a, 1);
      }
    });
  }

  CHECK(a.balance == 1000);
  CHECK(b.balance == 1000);
}

TEST_CASE("many readers, one writer") {
  Cache cache;
  cache.put("a", 1);
  cache.put("b", 2);

  std::atomic<int> total{0};
  {
    std::vector<std::jthread> readers;
    readers.reserve(static_cast<std::size_t>(8));
    for (int t = 0; t < 8; ++t) {
      readers.emplace_back([&cache, &total] {
        for (int i = 0; i < 1000; ++i) {
          total.fetch_add(cache.get("a"), std::memory_order_relaxed);
        }
      });
    }
  }

  CHECK(total.load() == 8000);
  CHECK(cache.get("b") == 2);
  CHECK(cache.get("missing") == -1);
  CHECK(cache.size() == 2);
}
