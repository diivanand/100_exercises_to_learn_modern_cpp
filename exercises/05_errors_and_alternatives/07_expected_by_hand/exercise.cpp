// =============================================================================
//  05.07 -- Result types: errors as values
// =============================================================================
//
//  Exceptions are the right tool when an error is exceptional and the handler
//  is far away. They are the wrong tool when failure is ORDINARY -- parsing
//  user input, looking something up, opening a file that may not exist. There,
//  an error is part of the function's normal output, and the type system
//  should say so.
//
//  `std::expected<T, E>` is C++23's answer. This exercise builds a small one,
//  because the parts are worth understanding and because you will meet
//  home-grown versions of it in most large C++ codebases.
//
//  The value: the caller CANNOT ignore the failure. There is no value to read
//  until they have dealt with the possibility that there is not one -- the
//  same property that makes std::optional (05.04) better than a sentinel,
//  with the reason attached.
//
//  The cost: errors do not propagate by themselves. Every layer has to pass
//  them along, which is why languages that do this well have a `?` operator
//  and C++ does not (yet). Weigh that against exceptions honestly; the answer
//  differs per codebase.
//
//  TASK
//    Implement `Result<T, E>` on top of std::variant, then use it in
//    `parse_port` and `configure` -- where you will feel exactly how much of
//    result-type programming is forwarding errors upward.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 05_07
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

// Tag types so that Result can tell a value from an error even when T and E
// are the same type. The standard library uses std::unexpected for this.
template <typename E>
struct Failure {
  E error;
};

template <typename E>
Failure(E) -> Failure<E>;

// TODO: implement Result.
//
//   * `Result(T value)` and `Result(Failure<E>)` constructors;
//   * `bool has_value()` and `explicit operator bool()`;
//   * `const T& value()` and `const E& error()`;
//   * `T value_or(T fallback)`.
//
// Storing `std::variant<T, E>` is enough for this exercise.
template <typename T, typename E>
class Result {
public:
private:
  std::variant<T, E> storage_;
};

enum class ParseError { kEmpty, kNotANumber, kOutOfRange };

std::string describe(ParseError error) {
  switch (error) {
  case ParseError::kEmpty:
    return "empty";
  case ParseError::kNotANumber:
    return "not a number";
  case ParseError::kOutOfRange:
    return "out of range";
  }
  return "unknown";
}

// Parses a TCP port: digits only, and in 1..65535.
//
// TODO: implement it, returning `Failure{ParseError::...}` for each failure
// and the port number on success.
Result<int, ParseError> parse_port(std::string_view text) {
  return Failure{ParseError::kEmpty};
}

struct Config {
  int http_port = 0;
  int https_port = 0;
};

// Parses both ports, failing with the FIRST error encountered.
//
// TODO: implement it. This is the part that shows the cost: each call has to
// be checked and its error forwarded. Notice how visible the error paths are
// compared with the exception version -- that visibility is the trade.
Result<Config, ParseError> configure(std::string_view http, std::string_view https) {
  return Failure{ParseError::kEmpty};
}

TEST_CASE("a successful parse carries the value") {
  const auto result = parse_port("8080");
  REQUIRE(result.has_value());
  CHECK(result.value() == 8080);
  CHECK(static_cast<bool>(result));
}

TEST_CASE("each failure carries its reason") {
  CHECK(parse_port("").error() == ParseError::kEmpty);
  CHECK(parse_port("80a").error() == ParseError::kNotANumber);
  CHECK(parse_port("0").error() == ParseError::kOutOfRange);
  CHECK(parse_port("70000").error() == ParseError::kOutOfRange);

  CHECK(describe(parse_port("").error()) == "empty");
}

TEST_CASE("value_or supplies a default without checking") {
  CHECK(parse_port("443").value_or(-1) == 443);
  CHECK(parse_port("nope").value_or(-1) == -1);
}

TEST_CASE("configure forwards the first error") {
  const auto good = configure("80", "443");
  REQUIRE(good.has_value());
  CHECK(good.value().http_port == 80);
  CHECK(good.value().https_port == 443);

  CHECK_FALSE(configure("", "443").has_value());
  CHECK(configure("", "443").error() == ParseError::kEmpty);

  // The second port is the one that fails here.
  CHECK(configure("80", "abc").error() == ParseError::kNotANumber);
}

TEST_CASE("a Result cannot be read without deciding what to do about failure") {
  // There is no implicit conversion to T, and no way to reach the value
  // without has_value(), value_or(), or a deliberate value() call. That is the
  // whole design: the caller is forced to look at the failure case.
  static_assert(!std::is_convertible_v<Result<int, ParseError>, int>);
  CHECK(true);
}
