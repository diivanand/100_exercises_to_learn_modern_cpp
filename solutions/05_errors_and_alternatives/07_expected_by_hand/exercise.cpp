// Solution -- 05.07 Result types
#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

template <typename E>
struct Failure {
  E error;
};

template <typename E>
Failure(E) -> Failure<E>;

template <typename T, typename E>
class Result {
public:
  Result(T value) : storage_(std::in_place_index<0>, std::move(value)) {}
  Result(Failure<E> failure)
      : storage_(std::in_place_index<1>, std::move(failure.error)) {}

  [[nodiscard]] bool has_value() const noexcept {
    return storage_.index() == 0;
  }
  explicit operator bool() const noexcept {
    return has_value();
  }

  [[nodiscard]] const T& value() const {
    return std::get<0>(storage_);
  }
  [[nodiscard]] const E& error() const {
    return std::get<1>(storage_);
  }

  [[nodiscard]] T value_or(T fallback) const {
    return has_value() ? value() : std::move(fallback);
  }

private:
  // in_place_index rather than the type, so that Result<int, int> still works.
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

Result<int, ParseError> parse_port(std::string_view text) {
  if (text.empty()) {
    return Failure{ParseError::kEmpty};
  }

  long value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') {
      return Failure{ParseError::kNotANumber};
    }
    value = value * 10 + (c - '0');
    if (value > 65535) {
      return Failure{ParseError::kOutOfRange};
    }
  }

  if (value < 1) {
    return Failure{ParseError::kOutOfRange};
  }
  return static_cast<int>(value);
}

struct Config {
  int http_port = 0;
  int https_port = 0;
};

Result<Config, ParseError> configure(std::string_view http, std::string_view https) {
  const auto http_port = parse_port(http);
  if (!http_port) {
    return Failure{http_port.error()};
  }

  const auto https_port = parse_port(https);
  if (!https_port) {
    return Failure{https_port.error()};
  }

  return Config{http_port.value(), https_port.value()};
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

  CHECK(configure("80", "abc").error() == ParseError::kNotANumber);
}

TEST_CASE("a Result cannot be read without deciding what to do about failure") {
  static_assert(!std::is_convertible_v<Result<int, ParseError>, int>);
  CHECK(true);
}
