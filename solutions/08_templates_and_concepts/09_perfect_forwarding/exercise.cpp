// Solution -- 08.09 Forwarding references and std::forward
#include <doctest/doctest.h>

#include <concepts>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

struct Tracked {
  Tracked() = default;
  explicit Tracked(std::string name) : name(std::move(name)) {}

  Tracked(const Tracked& other) : name(other.name) {
    ++copies_;
  }
  Tracked(Tracked&& other) noexcept : name(std::move(other.name)) {
    ++moves_;
  }
  Tracked& operator=(const Tracked&) = default;
  Tracked& operator=(Tracked&&) noexcept = default;
  ~Tracked() = default;

  std::string name;

  [[nodiscard]] static int copies() noexcept {
    return copies_;
  }
  [[nodiscard]] static int moves() noexcept {
    return moves_;
  }

  static void reset() {
    copies_ = 0;
    moves_ = 0;
  }

private:
  static inline int copies_ = 0;
  static inline int moves_ = 0;
};

struct Widget {
  Tracked payload;
  int count = 0;

  Widget(Tracked payload, int count) : payload(std::move(payload)), count(count) {}
};

template <typename... Args>
std::unique_ptr<Widget> create(Args&&... args) {
  // `std::forward<Args>(args)...` expands to a forward per argument, each
  // driven by its own deduced Args.
  return std::make_unique<Widget>(std::forward<Args>(args)...);
}

template <typename F, typename T>
void apply_twice(F&& f, T&& value) {
  f(value);                  // still needed afterwards: pass as an lvalue
  f(std::forward<T>(value)); // last use: give the category back
}

class Holder {
public:
  // The constraint is what stops this from being chosen for a Holder
  // argument -- including a non-const Holder lvalue, which would otherwise be
  // an exact match and beat the copy constructor.
  template <typename T>
    requires(!std::same_as<std::remove_cvref_t<T>, Holder>)
  explicit Holder(T&& value) : payload_(std::forward<T>(value)) {}

  Holder(const Holder&) = default;
  Holder(Holder&&) noexcept = default;
  Holder& operator=(const Holder&) = default;
  Holder& operator=(Holder&&) noexcept = default;
  ~Holder() = default;

  [[nodiscard]] const Tracked& payload() const noexcept {
    return payload_;
  }

private:
  Tracked payload_;
};

TEST_CASE("forwarding preserves the value category") {
  Tracked::reset();

  const auto moved = create(Tracked{"temp"}, 1);
  CHECK(moved->payload.name == "temp");
  CHECK(Tracked::copies() == 0);

  Tracked::reset();

  Tracked owned{"kept"};
  const auto copied = create(owned, 2);
  CHECK(copied->payload.name == "kept");
  CHECK(owned.name == "kept");
  CHECK(Tracked::copies() == 1);
}

TEST_CASE("forward only on the last use") {
  std::vector<std::string> seen;
  // By value, so that each call either copies or moves -- and the counters say
  // which.
  // NOLINTNEXTLINE(performance-unnecessary-value-param): by value on purpose
  const auto record = [&seen](Tracked value) { seen.push_back(value.name); };

  Tracked::reset();
  apply_twice(record, Tracked{"twice"});
  CHECK(seen == std::vector<std::string>{"twice", "twice"});
  // The first call must copy: `value` is still needed. The second is the last
  // use, so an rvalue argument can be moved into it.
  CHECK(Tracked::copies() == 1);
  CHECK(Tracked::moves() == 1);

  Tracked::reset();
  Tracked kept{"kept"};
  apply_twice(record, kept);
  // An lvalue belongs to the caller: neither call may move from it.
  CHECK(kept.name == "kept");
  CHECK(Tracked::copies() == 2);
  CHECK(Tracked::moves() == 0);
}

TEST_CASE("a forwarding constructor must not hijack copies") {
  const Tracked payload{"held"};
  Holder original{payload};

  Tracked::reset();

  const Holder copy{original};
  CHECK(copy.payload().name == "held");

  const Holder& const_reference = original;
  const Holder from_const{const_reference};
  CHECK(from_const.payload().name == "held");
}

TEST_CASE("reference collapsing, spelled out") {
  const auto deduce = []<typename T>(T&&) { return std::is_lvalue_reference_v<T>; };

  int lvalue = 0;
  CHECK(deduce(lvalue));
  CHECK_FALSE(deduce(0));
  // std::move on an int copies -- that is the point: `std::move` is a CAST,
  // not an operation, and casting an int to an rvalue still yields an int.
  // NOLINTNEXTLINE(performance-move-const-arg)
  CHECK_FALSE(deduce(std::move(lvalue)));
}
