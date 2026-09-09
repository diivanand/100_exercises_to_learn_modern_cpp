// Solution -- 03.06 RAII
#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Account {
  std::string name;
  int balance = 0;
};

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

class Transaction {
public:
  explicit Transaction(Ledger& ledger) : ledger_(ledger), snapshot_(ledger.snapshot()) {}

  // Rollback is the default; success is the thing you have to say out loud.
  ~Transaction() {
    if (!committed_) {
      ledger_.restore(std::move(snapshot_));
    }
  }

  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  Transaction(Transaction&&) = delete;
  Transaction& operator=(Transaction&&) = delete;

  void commit() noexcept {
    committed_ = true;
  }

private:
  Ledger& ledger_;
  std::vector<Account> snapshot_;
  bool committed_ = false;
};

void transfer(Ledger& ledger, const std::string& from, const std::string& to,
              int amount) {
  Transaction transaction{ledger};
  ledger.adjust(to, amount);
  ledger.adjust(from, -amount);
  transaction.commit();
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
      return;
    }
    transaction.commit();
  };

  maybe_charge(false);
  CHECK(ledger.balance_of("ada") == 100);

  maybe_charge(true);
  CHECK(ledger.balance_of("ada") == 90);
}
