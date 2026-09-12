// Solution -- 05.03 Scope guards
#include <doctest/doctest.h>

#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

template <typename Action>
class ScopeExit {
  // A destructor is implicitly noexcept, so an action that throws from it is
  // std::terminate. Saying so here turns that into a compile error, and tells
  // the caller to write `[&]() noexcept { ... }`.
  static_assert(std::is_nothrow_invocable_v<Action&>,
                "a scope guard's action must not throw");

public:
  explicit ScopeExit(Action action) : action_(std::move(action)) {}

  ~ScopeExit() {
    if (active_) {
      action_();
    }
  }

  ScopeExit(const ScopeExit&) = delete;
  ScopeExit& operator=(const ScopeExit&) = delete;
  ScopeExit(ScopeExit&&) = delete;
  ScopeExit& operator=(ScopeExit&&) = delete;

  void release() noexcept {
    active_ = false;
  }

private:
  Action action_;
  bool active_ = true;
};

template <typename Action>
class ScopeFail {
  static_assert(std::is_nothrow_invocable_v<Action&>,
                "a scope guard's action must not throw");

public:
  explicit ScopeFail(Action action)
      : action_(std::move(action)), exceptions_on_entry_(std::uncaught_exceptions()) {}

  ~ScopeFail() {
    // More exceptions in flight now than when we were constructed means this
    // scope is being unwound, not exited normally. Comparing counts (rather
    // than testing for non-zero) keeps this correct inside a destructor that
    // is itself running during someone else's unwinding.
    if (active_ && std::uncaught_exceptions() > exceptions_on_entry_) {
      action_();
    }
  }

  ScopeFail(const ScopeFail&) = delete;
  ScopeFail& operator=(const ScopeFail&) = delete;
  ScopeFail(ScopeFail&&) = delete;
  ScopeFail& operator=(ScopeFail&&) = delete;

  void release() noexcept {
    active_ = false;
  }

private:
  Action action_;
  int exceptions_on_entry_;
  bool active_ = true;
};

class Journal {
public:
  void begin(std::string name) {
    if (in_progress_) {
      throw std::logic_error{"already writing"};
    }
    in_progress_ = true;
    current_ = std::move(name);
  }

  void append(int value) {
    if (value < 0) {
      throw std::invalid_argument{"negative value"};
    }
    pending_.push_back(value);
  }

  void commit() {
    records_.push_back({current_, pending_});
    reset();
  }

  void abort() {
    reset();
  }

  struct Record {
    std::string name;
    std::vector<int> values;
  };

  const std::vector<Record>& records() const noexcept {
    return records_;
  }
  bool in_progress() const noexcept {
    return in_progress_;
  }

private:
  void reset() noexcept {
    in_progress_ = false;
    current_.clear();
    pending_.clear();
  }

  bool in_progress_ = false;
  std::string current_;
  std::vector<int> pending_;
  std::vector<Record> records_;
};

void write_record(Journal& journal, const std::string& name,
                  const std::vector<int>& values, int& attempts) {
  const ScopeExit count_attempt{[&attempts]() noexcept { ++attempts; }};

  journal.begin(name);
  const ScopeFail rollback{[&journal]() noexcept { journal.abort(); }};

  for (const int value : values) {
    journal.append(value);
  }
  journal.commit();
}

TEST_CASE("ScopeExit runs on the way out") {
  int calls = 0;
  {
    const ScopeExit guard{[&calls]() noexcept { ++calls; }};
    CHECK(calls == 0);
  }
  CHECK(calls == 1);
}

TEST_CASE("a released guard does nothing") {
  int calls = 0;
  {
    ScopeExit guard{[&calls]() noexcept { ++calls; }};
    guard.release();
  }
  CHECK(calls == 0);
}

TEST_CASE("ScopeExit runs even when an exception is propagating") {
  int calls = 0;
  CHECK_THROWS_AS(
      [&calls] {
        const ScopeExit guard{[&calls]() noexcept { ++calls; }};
        throw std::runtime_error{"boom"};
      }(),
      std::runtime_error);
  CHECK(calls == 1);
}

TEST_CASE("ScopeFail runs only on the failure path") {
  int failures = 0;

  {
    const ScopeFail guard{[&failures]() noexcept { ++failures; }};
  }
  CHECK(failures == 0);

  CHECK_THROWS_AS(
      [&failures] {
        const ScopeFail guard{[&failures]() noexcept { ++failures; }};
        throw std::runtime_error{"boom"};
      }(),
      std::runtime_error);
  CHECK(failures == 1);
}

TEST_CASE("write_record commits or aborts, and always counts the attempt") {
  Journal journal;
  int attempts = 0;

  write_record(journal, "good", {1, 2, 3}, attempts);
  CHECK(journal.records().size() == 1);
  CHECK(journal.in_progress() == false);
  CHECK(attempts == 1);

  CHECK_THROWS_AS(write_record(journal, "bad", {1, -1}, attempts), std::invalid_argument);
  CHECK(journal.records().size() == 1);
  // The journal must be usable again -- the aborted record left no trace.
  CHECK(journal.in_progress() == false);
  CHECK(attempts == 2);

  write_record(journal, "after", {9}, attempts);
  CHECK(journal.records().size() == 2);
  CHECK(attempts == 3);
}
