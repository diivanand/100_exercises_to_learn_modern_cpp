// Solution -- 04.03 Delegating and inheriting constructors
#include <doctest/doctest.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

class Connection {
public:
  // The one constructor that does the work. Everything else routes here, so
  // validation cannot be skipped by adding a fourth overload later.
  Connection(std::string host, int port, std::chrono::seconds timeout)
      : host_(std::move(host)), port_(port), timeout_(timeout) {
    validate();
  }

  Connection(std::string host, int port)
      : Connection(std::move(host), port, std::chrono::seconds{30}) {}

  explicit Connection(std::string host) : Connection(std::move(host), 80) {}

  [[nodiscard]] const std::string& host() const noexcept {
    return host_;
  }
  [[nodiscard]] int port() const noexcept {
    return port_;
  }
  [[nodiscard]] std::chrono::seconds timeout() const noexcept {
    return timeout_;
  }
  [[nodiscard]] int attempts() const noexcept {
    return attempts_;
  }

  void retry() {
    ++attempts_;
  }

private:
  void validate() const {
    if (host_.empty()) {
      throw std::invalid_argument{"host must not be empty"};
    }
    if (port_ <= 0 || port_ > 65535) {
      throw std::invalid_argument{"port out of range"};
    }
  }

  std::string host_;
  int port_;
  std::chrono::seconds timeout_;
  int attempts_ = 0;
};

class ProtocolError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

TEST_CASE("the defaults come from one place") {
  const Connection basic{"example.com"};
  CHECK(basic.host() == "example.com");
  CHECK(basic.port() == 80);
  CHECK(basic.timeout() == std::chrono::seconds{30});
  CHECK(basic.attempts() == 0);

  const Connection with_port{"example.com", 8080};
  CHECK(with_port.port() == 8080);
  CHECK(with_port.timeout() == std::chrono::seconds{30});

  const Connection full{"example.com", 443, std::chrono::seconds{5}};
  CHECK(full.port() == 443);
  CHECK(full.timeout() == std::chrono::seconds{5});
}

TEST_CASE("validation runs no matter which constructor was used") {
  CHECK_THROWS_AS(Connection{""}, std::invalid_argument);
  CHECK_THROWS_AS((Connection{"host", 0}), std::invalid_argument);
  CHECK_THROWS_AS((Connection{"host", 70000, std::chrono::seconds{1}}),
                  std::invalid_argument);
}

TEST_CASE("ProtocolError inherits every base constructor") {
  const ProtocolError from_string{std::string{"bad frame"}};
  CHECK(std::string{from_string.what()} == "bad frame");

  const ProtocolError from_literal{"bad opcode"};
  CHECK(std::string{from_literal.what()} == "bad opcode");

  try {
    throw ProtocolError{"thrown"};
  } catch (const std::runtime_error& error) {
    CHECK(std::string{error.what()} == "thrown");
  }
}
