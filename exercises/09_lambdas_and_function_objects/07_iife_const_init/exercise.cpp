// =============================================================================
//  09.07 -- Immediately-invoked lambdas
// =============================================================================
//
//  A lambda called on the spot:
//
//      const auto config = [] {
//        Config c;
//        c.load_defaults();
//        c.apply_overrides();
//        return c;
//      }();
//      //^^ note the parentheses
//
//  Why bother? Because it lets a variable that needs several statements to
//  set up still be `const`. Without it you write:
//
//      Config config;          // not const -- and briefly in a bogus state
//      config.load_defaults();
//      config.apply_overrides();
//
//  and now nothing stops later code from modifying it, and a reader has to
//  scan the whole function to find out whether anything does.
//
//  Core Guidelines Con.1: "By default, make objects immutable". The IIFE is
//  how you keep that default when initialisation is complicated -- a branch, a
//  loop, a switch, or a try/catch.
//
//  Two more uses:
//
//   * SCOPING TEMPORARIES. Everything the setup needed dies with the lambda,
//     so the enclosing scope stays clean.
//   * INITIALISING A const MEMBER in a constructor's member-initialiser list,
//     where statements are not allowed at all.
//
//  Do not overdo it: if the setup is reusable, it wants to be a named
//  function. The IIFE is for the one-off.
//
//  TASK
//    Turn the four mutable variables into const ones.
//
//
//  NOTE  This exercise starts as a compile error -- the last test asks
//        whether Report is copy-assignable, and it will not be once `title_`
//        is const.
//
//  RUN IT
//    ./mcpp test 09_07
//
// =============================================================================

#include <doctest/doctest.h>

#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

enum class Environment { kDevelopment, kStaging, kProduction };

struct Settings {
  std::string host;
  int port = 0;
  bool verbose = false;
};

// TODO: make `settings` const, using an immediately-invoked lambda around the
// switch.
Settings settings_for(Environment environment) {
  Settings settings;
  switch (environment) {
  case Environment::kDevelopment:
    settings = Settings{"localhost", 8080, true};
    break;
  case Environment::kStaging:
    settings = Settings{"staging.example.com", 443, true};
    break;
  case Environment::kProduction:
    settings = Settings{"example.com", 443, false};
    break;
  }
  return settings;
}

// TODO: make `lookup` const. Building a table from a list of pairs takes a
// loop, which is exactly the case the IIFE exists for.
std::map<std::string, int> build_index(const std::vector<std::string>& names) {
  std::map<std::string, int> lookup;
  int position = 0;
  for (const auto& name : names) {
    lookup[name] = position++;
  }
  return lookup;
}

class Report {
public:
  // TODO: `title_` is const, so it MUST be set in the member-initialiser list
  // -- there is no assigning to it in the body. An IIFE is the only way to run
  // several statements there.
  //
  // The title is "<name> (<count> rows)", or "<name> (empty)" when count is 0.
  explicit Report(std::string name, int count) {
    title_ = name;
    if (count == 0) {
      title_ += " (empty)";
    } else {
      title_ += " (" + std::to_string(count) + " rows)";
    }
  }

  const std::string& title() const noexcept {
    return title_;
  }

private:
  std::string title_;
};

// TODO: make the result const. The parse can throw, and the fallback path has
// two statements, so a ternary will not do.
int parse_with_fallback(const std::string& text, int fallback) {
  int value = 0;
  try {
    value = std::stoi(text);
  } catch (const std::exception&) {
    value = fallback;
  }
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
  // Once title_ is `const std::string`, Report is no longer assignable -- a
  // real cost, and the reason this is a judgement call rather than a rule.
  static_assert(!std::is_copy_assignable_v<Report>);
  static_assert(std::is_copy_constructible_v<Report>);
  CHECK(true);
}
