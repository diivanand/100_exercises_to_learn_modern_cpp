// Solution -- 05.02 The exception safety guarantees
#include <doctest/doctest.h>

#include <cstddef>
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

  int count_of(const std::string& item) const {
    const auto it = stock_.find(item);
    return it == stock_.end() ? 0 : it->second;
  }

  std::size_t distinct_items() const noexcept {
    return stock_.size();
  }

  void restock(const std::vector<Delivery>& deliveries) {
    // Everything that can throw -- the copy, the validation, the insertions --
    // happens before `stock_` is touched at all.
    std::map<std::string, int> updated = stock_;

    for (const auto& delivery : deliveries) {
      if (delivery.quantity <= 0) {
        throw std::invalid_argument{"quantity must be positive: " + delivery.item};
      }
      updated[delivery.item] += delivery.quantity;
    }

    // The commit. swap on a std::map is noexcept, so from here there is no
    // path that fails.
    stock_.swap(updated);
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
