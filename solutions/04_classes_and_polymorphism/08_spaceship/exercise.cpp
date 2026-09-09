// Solution -- 04.08 The three-way comparison operator
#include <doctest/doctest.h>

#include <algorithm>
#include <cctype>
#include <compare>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

struct Version {
  int major = 0;
  int minor = 0;
  int patch = 0;

  friend bool operator==(const Version&, const Version&) = default;
  friend std::strong_ordering operator<=>(const Version&, const Version&) = default;
};

class CaseInsensitive {
public:
  explicit CaseInsensitive(std::string value) : value_(std::move(value)) {}

  [[nodiscard]] const std::string& value() const noexcept {
    return value_;
  }

  friend std::weak_ordering operator<=>(const CaseInsensitive& lhs,
                                        const CaseInsensitive& rhs) {
    const std::size_t common = std::min(lhs.value_.size(), rhs.value_.size());
    for (std::size_t i = 0; i < common; ++i) {
      const char left = lower(lhs.value_[i]);
      const char right = lower(rhs.value_[i]);
      if (left != right) {
        return left <=> right;
      }
    }
    return lhs.value_.size() <=> rhs.value_.size();
  }

  friend bool operator==(const CaseInsensitive& lhs, const CaseInsensitive& rhs) {
    return (lhs <=> rhs) == std::weak_ordering::equivalent;
  }

private:
  // std::tolower is defined for values representable as unsigned char (and
  // EOF); passing a negative char straight through is undefined behaviour.
  static char lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  std::string value_;
};

TEST_CASE("Version gets all six operators from two defaults") {
  constexpr Version v1{1, 2, 3};
  constexpr Version v2{1, 3, 0};

  CHECK(v1 < v2);
  CHECK(v1 <= v2);
  CHECK(v2 > v1);
  CHECK(v2 >= v1);
  CHECK(v1 != v2);
  CHECK(v1 == Version{1, 2, 3});

  CHECK(Version{2, 0, 0} > Version{1, 99, 99});
}

TEST_CASE("the comparison category is strong") {
  static_assert(std::is_same_v<decltype(Version{} <=> Version{}), std::strong_ordering>);
  CHECK((Version{1, 0, 0} <=> Version{1, 0, 0}) == std::strong_ordering::equal);
  CHECK(true);
}

TEST_CASE("sorting comes for free") {
  std::vector<Version> versions = {{1, 10, 0}, {1, 2, 0}, {0, 9, 9}, {1, 2, 1}};
  std::ranges::sort(versions);
  CHECK(versions.front() == Version{0, 9, 9});
  CHECK(versions.back() == Version{1, 10, 0});
}

TEST_CASE("CaseInsensitive is weakly ordered") {
  const CaseInsensitive upper{"HELLO"};
  const CaseInsensitive lower{"hello"};

  CHECK(upper == lower);
  CHECK((upper <=> lower) == std::weak_ordering::equivalent);
  CHECK(upper.value() != lower.value());

  static_assert(std::is_same_v<decltype(std::declval<CaseInsensitive>() <=>
                                        std::declval<CaseInsensitive>()),
                               std::weak_ordering>);
}

TEST_CASE("CaseInsensitive orders like a dictionary") {
  CHECK(CaseInsensitive{"apple"} < CaseInsensitive{"Banana"});
  CHECK(CaseInsensitive{"Zebra"} > CaseInsensitive{"apple"});
  CHECK(CaseInsensitive{"pre"} < CaseInsensitive{"prefix"});
}
