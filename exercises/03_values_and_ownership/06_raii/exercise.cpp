// =============================================================================
//  03.06 -- RAII: the idea the whole language is built around
// =============================================================================
//
//  Resource Acquisition Is Initialisation. A bad name for a simple idea:
//
//      tie the lifetime of a resource to the lifetime of an object,
//      so that releasing it is the destructor's job and nobody else's.
//
//  Why it beats `try`/`finally` and "remember to call cleanup()":
//
//   * There is no path out of a scope that skips destructors. Not `return`,
//     not `break`, not an exception thrown three frames down.
//   * The release code is written once, next to the acquisition, instead of
//     once per exit path.
//   * It composes. An object holding five RAII members releases five
//     resources, in reverse order of construction, with no code at all.
//
//  This is why C++ has no `finally`: it does not need one.
//
//  Core Guidelines R.1: "Manage resources automatically using resource handles
//  and RAII".
//
//  TASK
//    `Transaction` currently rolls back only on the paths somebody remembered.
//    Turn it into an RAII type: commit is explicit, rollback is automatic.
//    Then use it to fix `transfer`, which leaks a half-applied transfer when
//    the withdrawal fails.
//
//  RUN IT
//    ./mcpp test 03_06
//
// =============================================================================

#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <vector>

struct Account {
  std::string name;
  int balance = 0;
};

// A ledger that can be rolled back to a saved state.
class Ledger {
public:
  explicit Ledger(std::vector<Account> accounts) : accounts_(std::move(accounts)) {}

  [[nodiscard]] int balance_of(const std::string& name) const {
    for (const auto& account : accounts_) {
      if (account.name == name) {
        return account.balance;
      }
    }
    throw std::out_of_range{"no such account: " + name};
  }

  void adjust(const std::string& name, int amount) {
    for (auto& account : accounts_) {
      if (account.name == name) {
        if (account.balance + amount < 0) {
          throw std::runtime_error{"insufficient funds"};
        }
        account.balance += amount;
        return;
      }
    }
    throw std::out_of_range{"no such account: " + name};
  }

  [[nodiscard]] std::vector<Account> snapshot() const {
    return accounts_;
  }
  void restore(std::vector<Account> snapshot) {
    accounts_ = std::move(snapshot);
  }

private:
  std::vector<Account> accounts_;
};

// TODO: make this an RAII scope guard.
//
//   * The constructor takes the snapshot.
//   * `commit()` marks the transaction as successful.
//   * The DESTRUCTOR restores the snapshot unless commit() was called.
//   * It must not be copyable -- two guards restoring the same snapshot is
//     nonsense. Follow the rule of five (03.05) and delete what does not
//     apply.
//
// A destructor must not throw: if it did while an exception was already in
// flight, the program terminates. Keep the rollback simple enough that it
// cannot fail (Core Guidelines C.36, C.37).
class Transaction {
public:
  explicit Transaction(Ledger& ledger) : ledger_(ledger), snapshot_(ledger.snapshot()) {}

  void commit() {}

private:
  // Already written for you -- the exercise is deciding *when* this runs.
  void rollback() {
    ledger_.restore(snapshot_);
  }

  Ledger& ledger_;
  std::vector<Account> snapshot_;
};

// Moves `amount` from one account to another. Either both sides happen or
// neither does.
//
// TODO: rewrite using a Transaction so that a failure anywhere -- including
// the exception `adjust` throws for insufficient funds -- leaves the ledger
// untouched. Notice that you do not need a single `catch`.
void transfer(Ledger& ledger, const std::string& from, const std::string& to,
              int amount) {
  ledger.adjust(to, amount);
  ledger.adjust(from, -amount);
}

TEST_CASE("a successful transfer moves the money") {
  Ledger ledger{{{"ada", 100}, {"alan", 50}}};
  transfer(ledger, "ada", "alan", 30);
  CHECK(ledger.balance_of("ada") == 70);
  CHECK(ledger.balance_of("alan") == 80);
}

TEST_CASE("a failed transfer changes nothing") {
  Ledger ledger{{{"ada", 10}, {"alan", 50}}};

  CHECK_THROWS_AS(transfer(ledger, "ada", "alan", 30), std::runtime_error);

  // Without the guard, "alan" has already been credited when the withdrawal
  // throws -- money created out of nothing.
  CHECK(ledger.balance_of("ada") == 10);
  CHECK(ledger.balance_of("alan") == 50);
}

TEST_CASE("an unknown account rolls back too") {
  Ledger ledger{{{"ada", 100}}};
  CHECK_THROWS_AS(transfer(ledger, "ada", "nobody", 10), std::out_of_range);
  CHECK(ledger.balance_of("ada") == 100);
}

TEST_CASE("the guard rolls back on an early return, not just on a throw") {
  Ledger ledger{{{"ada", 100}}};

  const auto maybe_charge = [&ledger](bool proceed) {
    Transaction transaction{ledger};
    ledger.adjust("ada", -10);
    if (!proceed) {
      return; // no commit, no explicit rollback, no leak
    }
    transaction.commit();
  };

  maybe_charge(false);
  CHECK(ledger.balance_of("ada") == 100);

  maybe_charge(true);
  CHECK(ledger.balance_of("ada") == 90);
}
