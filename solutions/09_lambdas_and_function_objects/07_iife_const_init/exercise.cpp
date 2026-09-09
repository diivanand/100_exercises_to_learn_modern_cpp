// Solution -- 09.07 Immediately-invoked lambdas
#include <doctest/doctest.h>

#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

enum class Environment { kDevelopment, kStaging, kProduction };

struct Settings {
  std::string host;
  int port = 0;
  bool verbose = false;
};

Settings settings_for(Environment environment) {
  const Settings settings = [environment] {
    switch (environment) {
    case Environment::kDevelopment:
      return Settings{"localhost", 8080, true};
    case Environment::kStaging:
      return Settings{"staging.example.com", 443, true};
    case Environment::kProduction:
      return Settings{"example.com", 443, false};
    }
    return Settings{};
  }();
  return settings;
}

std::map<std::string, int> build_index(const std::vector<std::string>& names) {
  const std::map<std::string, int> lookup = [&names] {
    std::map<std::string, int> table;
    int position = 0;
    for (const auto& name : names) {
      table[name] = position++;
    }
    return table;
  }();
  return lookup;
}

class Report {
public:
  // A const member can only be set in the member-initialiser list, and a
  // member-initialiser list holds expressions, not statements. An
  // immediately-invoked lambda is an expression that contains statements --
  // which is exactly the gap it fills.
  explicit Report(std::string name, int count)
      : title_([&name, count] {
          std::string title = std::move(name);
          if (count == 0) {
            title += " (empty)";
          } else {
            title += " (" + std::to_string(count) + " rows)";
          }
          return title;
        }()) {}

  [[nodiscard]] const std::string& title() const noexcept {
    return title_;
  }

private:
  // A const member is what this exercise is about: it forces the value to be
  // built in the initialiser list, which is why the IIFE is there. The cost is
  // that Report is no longer assignable -- the last test checks exactly that,
  // so it is the trade-off rather than an oversight.
  const std::string title_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

int parse_with_fallback(const std::string& text, int fallback) {
  const int value = [&text, fallback] {
    try {
      return std::stoi(text);
    } catch (const std::exception&) {
      return fallback;
    }
  }();
  return value;
}

TEST_CASE("settings_for") {
  CHECK(settings_for(Environment::kDevelopment).host == "localhost");
  CHECK(settings_for(Environment::kDevelopment).port == 8080);
  CHECK(settings_for(Environment::kProduction).verbose == false);
  CHECK(settings_for(Environment::kStaging).host == "staging.example.com");
}

TEST_CASE("build_index") {
  const auto index = build_index({"a", "b", "c"});
  CHECK(index.at("a") == 0);
  CHECK(index.at("c") == 2);
  CHECK(build_index({}).empty());
}

TEST_CASE("a const member built in the initialiser list") {
  const Report empty{"Sales", 0};
  CHECK(empty.title() == "Sales (empty)");

  const Report full{"Sales", 42};
  CHECK(full.title() == "Sales (42 rows)");
}

TEST_CASE("parse_with_fallback") {
  CHECK(parse_with_fallback("123", -1) == 123);
  CHECK(parse_with_fallback("oops", -1) == -1);
  CHECK(parse_with_fallback("", 7) == 7);
}

TEST_CASE("the title member really is const") {
  static_assert(!std::is_copy_assignable_v<Report>);
  static_assert(std::is_copy_constructible_v<Report>);
  CHECK(true);
}
