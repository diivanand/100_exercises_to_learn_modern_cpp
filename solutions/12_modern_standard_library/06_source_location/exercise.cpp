// Solution -- 12.06 std::source_location
#include <doctest/doctest.h>

#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

std::vector<std::string> log_lines;

void log_impl(std::string_view message, const char* file, unsigned line) {
  const std::string_view path{file};
  const auto slash = path.find_last_of('/');
  const std::string_view name =
      slash == std::string_view::npos ? path : path.substr(slash + 1);
  log_lines.emplace_back(std::string{name} + ":" + std::to_string(line) + " " +
                         std::string{message});
}

// `current()` as a default argument is evaluated where the call is written, so
// this ordinary function reports the caller's position exactly as the macro
// did -- and unlike the macro it can be namespaced, overloaded and passed on.
void log(std::string_view message,
         std::source_location where = std::source_location::current()) {
  log_impl(message, where.file_name(), where.line());
}

class TracedError : public std::runtime_error {
public:
  // The location parameter must come last and carry the default, which is why
  // a type like this cannot also be variadic without extra machinery.
  explicit TracedError(const std::string& message,
                       std::source_location where = std::source_location::current())
      : std::runtime_error(message), where_(where) {}

  [[nodiscard]] const std::source_location& where() const noexcept {
    return where_;
  }

private:
  std::source_location where_;
};

void require_positive(int value,
                      std::source_location where = std::source_location::current()) {
  if (value <= 0) {
    // Passing the caller's location on, rather than letting a fresh
    // `current()` default point at this line.
    throw TracedError{"value must be positive", where};
  }
}

TEST_CASE("a function replaces the macro") {
  log_lines.clear();

  log("started");
  const unsigned expected_line = std::source_location::current().line() - 1;

  REQUIRE(log_lines.size() == 1);
  CHECK(log_lines[0] == "exercise.cpp:" + std::to_string(expected_line) + " started");
}

TEST_CASE("the location is the call site, not the logging function") {
  log_lines.clear();

  const auto helper = [] { log("from a lambda"); };
  helper();

  REQUIRE(log_lines.size() == 1);
  CHECK(log_lines[0].starts_with("exercise.cpp:"));
  CHECK(log_lines[0].ends_with(" from a lambda"));
}

TEST_CASE("an error records where it was made") {
  const TracedError error{"something went wrong"};
  const unsigned expected_line = std::source_location::current().line() - 1;

  CHECK(std::string{error.what()} == "something went wrong");
  CHECK(error.where().line() == expected_line);
  CHECK(std::string_view{error.where().file_name()}.ends_with("exercise.cpp"));
}

TEST_CASE("passing the location on names the real caller") {
  // Taken two lines above the call, so the expected line is this one plus two.
  const std::source_location call_site = std::source_location::current();
  try {
    require_positive(-1);
    FAIL("expected a throw");
  } catch (const TracedError& error) {
    // The location must be the `require_positive(-1)` call above, not the
    // `throw` inside require_positive: the line is the call's, and the
    // function is this test, not require_positive.
    CHECK(error.where().line() == call_site.line() + 2);
    CHECK(std::string_view{error.where().function_name()}.find("require_positive") ==
          std::string_view::npos);
  }

  CHECK_NOTHROW(require_positive(1));
}
