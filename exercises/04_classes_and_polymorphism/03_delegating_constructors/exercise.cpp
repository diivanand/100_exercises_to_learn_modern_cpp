// =============================================================================
//  04.03 -- Delegating and inheriting constructors
// =============================================================================
//
//  Before C++11, a class with four constructors had four copies of the same
//  initialisation logic, usually funnelled through a private `init()` that ran
//  *after* the members had already been default-constructed.
//
//  DELEGATING CONSTRUCTORS let one constructor call another:
//
//      Connection(std::string host, int port, Timeout t) : ... { }
//      Connection(std::string host) : Connection(std::move(host), 80, ...) {}
//
//  One constructor holds the logic; the rest supply defaults. Note that a
//  delegating constructor cannot also have a member initialiser list -- it
//  delegates, then runs its own body.
//
//  INHERITING CONSTRUCTORS pull a base class's constructors into a derived
//  class with one line:
//
//      class SpecificError : public std::runtime_error {
//        using std::runtime_error::runtime_error;   // all of them
//      };
//
//  DEFAULT MEMBER INITIALISERS are the third piece: a member with `= value`
//  in the class body needs no mention in any constructor. Between the three,
//  most classes need one real constructor and no repetition.
//
//  TASK
//    Collapse `Connection`'s three constructors down to one that does the work
//    plus two that delegate -- which fixes the bug one of them already has --
//    and give `ProtocolError` the constructors of its base.
//
//  RUN IT
//    ./mcpp test 04_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

class Connection {
public:
  // TODO: rewrite these three so that only the first one has a body, and the
  // other two delegate to it. Keep the same defaults: port 80, timeout 30s,
  // and `attempts_` always starts at 0.
  explicit Connection(std::string host)
      : host_(std::move(host)), port_(80), timeout_(30), attempts_(0) {
    validate();
  }

  Connection(std::string host, int port)
      : host_(std::move(host)), port_(port), timeout_(30), attempts_(0) {
    // Somebody added this overload later and forgot the validate() call. That
    // is what duplicated construction logic costs you, and no amount of care
    // prevents it -- only having one place to forget.
  }

  Connection(std::string host, int port, std::chrono::seconds timeout)
      : host_(std::move(host)), port_(port), timeout_(timeout), attempts_(0) {
    validate();
  }

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
  // TODO: this is the same in every constructor. Give it a default member
  // initialiser and stop mentioning it.
  int attempts_;
};

// TODO: give ProtocolError all of std::runtime_error's constructors with a
// single `using` declaration, instead of forwarding them one at a time.
class ProtocolError : public std::runtime_error {
public:
  explicit ProtocolError(const std::string& message) : std::runtime_error(message) {}
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

  // std::runtime_error also has a const char* constructor. Forwarding them by
  // hand means remembering every one; `using` means you cannot forget.
  const ProtocolError from_literal{"bad opcode"};
  CHECK(std::string{from_literal.what()} == "bad opcode");

  // It is still an exception, so it is still catchable as one.
  try {
    throw ProtocolError{"thrown"};
  } catch (const std::runtime_error& error) {
    CHECK(std::string{error.what()} == "thrown");
  }
}
