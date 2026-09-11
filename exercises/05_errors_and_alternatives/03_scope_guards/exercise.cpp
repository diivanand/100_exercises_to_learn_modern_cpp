// =============================================================================
//  05.03 -- Scope guards
// =============================================================================
//
//  RAII (03.06) works when there is a natural owner type. Sometimes there is
//  not: you need to run *some code* on the way out of a scope, and writing a
//  whole class for it is more ceremony than the job deserves.
//
//  A scope guard is that class, written once, parameterised on a callable:
//
//      auto guard = ScopeExit{[&] { close(handle); }};
//
//  The Library Fundamentals TS calls these `scope_exit`, `scope_fail` and
//  `scope_success`; they are not in C++20's standard library, so you write the
//  ten lines yourself. Being able to is the point of this exercise.
//
//  Three design details that are not obvious:
//
//   * The destructor must not throw. Swallowing with a try/catch hides the
//     failure; the honest choice is to require a callable that cannot throw,
//     and say so with a `static_assert` on `std::is_nothrow_invocable_v`, so
//     a throwing action is a compile error rather than a std::terminate.
//   * A guard must not be copyable: two guards running the same action is
//     wrong. Move is optional; deleting all four is simplest and enough.
//   * `release()` (or `dismiss()`) lets the happy path cancel the action --
//     this is how a commit/rollback guard is built (03.06).
//
//  `std::uncaught_exceptions()` is what makes scope_fail and scope_success
//  possible: it returns the number of exceptions currently propagating, so a
//  destructor can tell why it is running.
//
//  TASK
//    Implement `ScopeExit` and `ScopeFail`, then use them in `write_record`.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 05_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Runs its action when the scope ends, whatever the reason.
//
// TODO: implement it.
//   * a constructor taking the action by value;
//   * a destructor that runs the action unless it was released;
//   * `release()` to cancel;
//   * copy and move deleted.
//
// The class template argument is deduced from the constructor, so callers
// write `ScopeExit{[&] { ... }}` with no explicit type (that is C++17 class
// template argument deduction -- 08.02).
template <typename Action>
class ScopeExit {
public:
  explicit ScopeExit(Action action) : action_(std::move(action)) {}

private:
  Action action_;
};

// Runs its action only if the scope is being left because of an exception.
//
// TODO: implement it. Record `std::uncaught_exceptions()` in the constructor;
// in the destructor, run the action only if the count is now higher.
//
// (Comparing against the constructor's count, rather than checking for
// non-zero, is what makes this correct inside a destructor that is itself
// running during unwinding.)
template <typename Action>
class ScopeFail {
public:
  explicit ScopeFail(Action action) : action_(std::move(action)) {}

private:
  Action action_;
};

// A journal that must never be left with a half-written record.
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

  [[nodiscard]] const std::vector<Record>& records() const noexcept {
    return records_;
  }
  [[nodiscard]] bool in_progress() const noexcept {
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

// TODO: rewrite using the guards.
//
//   * a ScopeFail that calls `journal.abort()` -- so a bad value cannot leave
//     the journal mid-record;
//   * an unconditional ScopeExit that increments `attempts`, so the counter is
//     right whether the write succeeded or not.
//
// Notice there is no try/catch and no duplicated cleanup on the error path.
void write_record(Journal& journal, const std::string& name,
                  const std::vector<int>& values, int& attempts) {
  journal.begin(name);
  for (const int value : values) {
    journal.append(value);
  }
  journal.commit();
  ++attempts;
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
