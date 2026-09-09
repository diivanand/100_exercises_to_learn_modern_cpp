// =============================================================================
//  10.03 -- Mutexes, lock guards, and deadlock
// =============================================================================
//
//  NEVER LOCK A MUTEX BY HAND. `mutex.lock()` followed by `mutex.unlock()` is
//  wrong the moment anything between them throws or returns early -- and it
//  always eventually does. Use an RAII lock (03.06):
//
//      std::lock_guard   lock and unlock. The default.
//      std::unique_lock  lock_guard plus: deferred locking, early unlock,
//                        moving, and use with a condition_variable (10.04).
//      std::scoped_lock  locks SEVERAL mutexes at once, without deadlock.
//      std::shared_lock  a reader's lock on a std::shared_mutex.
//
//  DEADLOCK needs four conditions, and the easy one to remove is inconsistent
//  ordering. Two threads that lock A then B, and B then A, will eventually
//  meet in the middle and stop forever.
//
//  Two ways out:
//
//   1. `std::scoped_lock lock{a, b};` -- it uses a deadlock-avoiding
//      algorithm, so the order you write does not matter. Use it whenever you
//      need two locks at once.
//   2. Always acquire in the same global order (by address, by id, by rank).
//
//  Also: DO NOT CALL UNKNOWN CODE WHILE HOLDING A LOCK. A callback that locks
//  something else, or that calls back into you, is how the ordering rule gets
//  broken without anyone noticing.
//
//  `std::shared_mutex` lets many readers in at once but only one writer, which
//  pays off when reads greatly outnumber writes -- and costs more than a plain
//  mutex when they do not.
//
//  TASK
//    Fix the manual locking, the deadlock, and the reader-writer bottleneck.
//
//  RUN IT
//    ./mcpp test 10_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <atomic>
#include <cstddef>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// TODO: replace the manual lock/unlock with a std::lock_guard. As written, the
// throw leaves the mutex locked forever, and the next caller blocks for good.
class Queue {
public:
  void push(int value) {
    mutex_.lock();
    if (value < 0) {
      throw std::invalid_argument{"negative"};
    }
    items_.push_back(value);
    mutex_.unlock();
  }

  [[nodiscard]] std::size_t size() {
    const std::lock_guard lock{mutex_};
    return items_.size();
  }

private:
  std::mutex mutex_;
  std::vector<int> items_;
};

// Two accounts, and a transfer that locks both.
struct Account {
  std::mutex mutex;
  int balance = 0;
};

// TODO: this locks `from` then `to`. Two threads transferring in opposite
// directions deadlock. Use std::scoped_lock to take both at once.
void transfer(Account& from, Account& to, int amount) {
  const std::lock_guard first{from.mutex};
  const std::lock_guard second{to.mutex};
  from.balance -= amount;
  to.balance += amount;
}

// A cache read far more often than it is written.
//
// TODO: use std::shared_mutex -- std::shared_lock for the readers,
// std::unique_lock (or lock_guard) for the writer -- so that concurrent
// lookups do not serialise behind each other.
class Cache {
public:
  void put(std::string key, int value) {
    const std::lock_guard lock{mutex_};
    entries_.insert_or_assign(std::move(key), value);
  }

  [[nodiscard]] int get(const std::string& key) const {
    const std::lock_guard lock{mutex_};
    const auto it = entries_.find(key);
    return it == entries_.end() ? -1 : it->second;
  }

  [[nodiscard]] std::size_t size() const {
    const std::lock_guard lock{mutex_};
    return entries_.size();
  }

private:
  mutable std::mutex mutex_;
  std::map<std::string, int> entries_;
};

TEST_CASE("a throw must not leave the mutex locked") {
  Queue queue;
  queue.push(1);
  CHECK_THROWS_AS(queue.push(-1), std::invalid_argument);

  // With a manual unlock, this call blocks forever and the test never
  // finishes. With a lock_guard, the mutex was released as the exception
  // unwound the scope.
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
