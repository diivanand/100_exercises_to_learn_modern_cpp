// =============================================================================
//  05.02 -- The exception safety guarantees
// =============================================================================
//
//  Any function that can throw makes one of four promises. Knowing which one
//  you are making is the difference between "handles errors" and "corrupts
//  state on errors".
//
//    NOTHROW      it will not throw. (`noexcept`, swap, destructors.)
//
//    STRONG       if it throws, nothing changed. The operation either
//                 completes or has no effect -- transactional.
//
//    BASIC        if it throws, everything is still valid and destructible,
//                 and no resource leaked -- but the value may have changed.
//                 This is the minimum acceptable guarantee.
//
//    NONE         all bets are off. Not acceptable in a library.
//
//  The standard library documents these. `vector::push_back` is strong;
//  `vector::insert` in the middle is basic-with-conditions; `swap` is nothrow.
//
//  HOW TO GET THE STRONG GUARANTEE: do all the work that can throw on a COPY,
//  then commit with operations that cannot throw. That is the copy-and-swap
//  idiom (03.01) generalised, and it is why `swap` being noexcept matters so
//  much.
//
//  TASK
//    `Inventory::restock` currently offers no guarantee at all: a throw
//    halfway through leaves the inventory half-updated. Give it the strong
//    guarantee.
//
//  RUN IT
//    ./mcpp test 05_02
//
// =============================================================================

#include <doctest/doctest.h>

#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Delivery {
  std::string item;
  int quantity = 0;
};

class Inventory {
public:
  Inventory() = default;
  explicit Inventory(std::map<std::string, int> stock) : stock_(std::move(stock)) {}

  [[nodiscard]] int count_of(const std::string& item) const {
    const auto it = stock_.find(item);
    return it == stock_.end() ? 0 : it->second;
  }

  [[nodiscard]] std::size_t distinct_items() const noexcept {
    return stock_.size();
  }

  // Applies every delivery, or none of them.
  //
  // TODO: give this the strong guarantee. The current version mutates `stock_`
  // as it goes, so a rejected delivery halfway through leaves the earlier ones
  // applied.
  //
  // The shape of the fix:
  //   1. copy stock_ into a local (this may throw -- nothing has changed yet);
  //   2. do all the validation and mutation on the copy;
  //   3. commit with `stock_.swap(updated)`, which cannot throw.
  void restock(const std::vector<Delivery>& deliveries) {
    for (const auto& delivery : deliveries) {
      if (delivery.quantity <= 0) {
        throw std::invalid_argument{"quantity must be positive: " + delivery.item};
      }
      stock_[delivery.item] += delivery.quantity;
    }
  }

private:
  std::map<std::string, int> stock_;
};

TEST_CASE("a successful restock applies everything") {
  Inventory inventory{{{"bolt", 10}}};
  inventory.restock({{"bolt", 5}, {"nut", 20}});

  CHECK(inventory.count_of("bolt") == 15);
  CHECK(inventory.count_of("nut") == 20);
  CHECK(inventory.distinct_items() == 2);
}

TEST_CASE("a rejected delivery leaves the inventory exactly as it was") {
  Inventory inventory{{{"bolt", 10}}};

  CHECK_THROWS_AS(inventory.restock({{"bolt", 5}, {"nut", -1}}), std::invalid_argument);

  // Without the strong guarantee, "bolt" is already 15 and "nut" already
  // exists with value 0 -- created by operator[] on the way past.
  CHECK(inventory.count_of("bolt") == 10);
  CHECK(inventory.count_of("nut") == 0);
  CHECK(inventory.distinct_items() == 1);
}

TEST_CASE("the failure can be the very first delivery") {
  Inventory inventory{{{"bolt", 10}}};
  CHECK_THROWS_AS(inventory.restock({{"bolt", 0}}), std::invalid_argument);
  CHECK(inventory.count_of("bolt") == 10);
}

TEST_CASE("an empty restock is a no-op") {
  Inventory inventory{{{"bolt", 10}}};
  inventory.restock({});
  CHECK(inventory.count_of("bolt") == 10);
}
