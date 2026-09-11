// =============================================================================
//  12.06 -- std::source_location
// =============================================================================
//
//  For decades, "where did this log line come from?" meant a macro:
//
//      #define LOG(msg) log_impl(msg, __FILE__, __LINE__)
//
//  Macros do not respect scope, do not respect namespaces, and cannot be
//  overloaded or passed around. `std::source_location` replaces this one with
//  an ordinary function:
//
//      void log(std::string_view message,
//               std::source_location where = std::source_location::current());
//
//  The trick is that `current()` as a DEFAULT ARGUMENT is evaluated at the
//  CALL SITE, not in the function body -- so it reports the caller's file,
//  line, column and function name.
//
//      where.file_name()      const char*
//      where.line()           unsigned
//      where.column()         unsigned
//      where.function_name()  const char*
//
//  Where this matters: logging, assertions, tracing, and error types that want
//  to record where they were constructed. It composes: pass the location on
//  explicitly and an inner function can report its caller's caller.
//
//  The one rule to remember: THE SOURCE_LOCATION PARAMETER MUST BE LAST, and
//  must have the default. Which means a function that also wants variadic
//  arguments has to take the location first, wrapped in a small struct -- the
//  same trick `std::format`'s `format_string` uses.
//
//  TASK
//    Replace the macro, and give the error type a location.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 12_06
//
// =============================================================================

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

// TODO: replace this macro with a function taking a defaulted
// std::source_location. A macro cannot be namespaced, cannot be overloaded,
// and its name collides with everything.
#define LOG(message) log_impl((message), __FILE__, __LINE__)

// An error that remembers where it was constructed.
//
// TODO: add a std::source_location member, defaulted in the constructor, and
// a `where()` accessor. Note that the location parameter has to come LAST.
class TracedError : public std::runtime_error {
public:
  explicit TracedError(const std::string& message) : std::runtime_error(message) {}
};

// Validates a value, throwing a TracedError that names the CALLER's line.
//
// TODO: take a source_location parameter and pass it on, so the error points
// at whoever called `require_positive` rather than at this function.
void require_positive(int value) {
  if (value <= 0) {
    throw TracedError{"value must be positive"};
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
  // The file is this one, and the line is the one inside the lambda -- not
  // wherever `log` itself is defined.
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
