// =============================================================================
//  05.01 -- Exceptions
// =============================================================================
//
//  An exception is for an error that the immediate caller cannot sensibly
//  handle and cannot be expected to check for. That is the test: not "is this
//  bad", but "would checking a return value here be noise?"
//
//  The rules that matter (Core Guidelines E.2, E.14, E.15):
//
//   * THROW BY VALUE, CATCH BY REFERENCE TO CONST. Catching by value slices a
//     derived exception down to its base and loses the interesting part.
//
//   * DERIVE FROM std::exception, usually from std::runtime_error or
//     std::logic_error. Throwing an int or a string means every catch site has
//     to guess.
//
//   * ORDER YOUR CATCH BLOCKS MOST-DERIVED FIRST. They are tried in order, and
//     a `catch (const std::exception&)` before a specific one swallows it.
//     Some compilers warn; do not rely on it.
//
//   * `throw;` RETHROWS the current exception, preserving its dynamic type.
//     `throw error;` copies -- and slices, if `error` was caught by base
//     reference.
//
//   * A DESTRUCTOR MUST NOT THROW. If it does while another exception is
//     unwinding, the program terminates.
//
//  TASK
//    Fix `parse_config`, whose error handling loses information three
//    different ways.
//
//
//  NOTE  This exercise starts as a compile error, and the diagnostic is the
//        third bug: clang notices that the MissingKey handler is unreachable
//        because the ConfigError one comes first.
//
//  RUN IT
//    ./mcpp test 05_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <map>
#include <stdexcept>
#include <string>

// The error hierarchy. `ConfigError` is the family; the two below are the
// specific reasons.
class ConfigError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class MissingKey : public ConfigError {
public:
  explicit MissingKey(const std::string& key)
      : ConfigError{"missing key: " + key}, key_{key} {}

  const std::string& key() const noexcept {
    return key_;
  }

private:
  std::string key_;
};

class BadValue : public ConfigError {
public:
  BadValue(const std::string& key, const std::string& value)
      : ConfigError{"bad value for " + key + ": " + value} {}
};

int require_int(const std::map<std::string, std::string>& config,
                const std::string& key) {
  const auto it = config.find(key);
  if (it == config.end()) {
    throw MissingKey{key};
  }
  try {
    return std::stoi(it->second);
  } catch (const std::invalid_argument&) {
    throw BadValue{key, it->second};
  } catch (const std::out_of_range&) {
    throw BadValue{key, it->second};
  }
}

// Reads `section.key`, and lets any failure through to the caller with its
// type and message intact. (A real system might log here first; what it must
// not do is change what the caller catches.)
//
// TODO: three bugs.
//
//  1. It catches by value, which slices a MissingKey down to a ConfigError --
//     `key()` is gone before the handler runs.
//  2. `throw error;` copies the sliced object rather than rethrowing the
//     original. Use a bare `throw;`.
//  3. The catch blocks are in the wrong order: the base class first means the
//     derived handler is unreachable.
int read_setting(const std::map<std::string, std::string>& config,
                 const std::string& section, const std::string& key) {
  try {
    return require_int(config, section + "." + key);
  } catch (ConfigError error) {
    throw error;
  } catch (const MissingKey&) {
    return 0;
  }
}

TEST_CASE("a missing key keeps its type and its detail") {
  const std::map<std::string, std::string> config = {{"net.port", "8080"}};

  CHECK(read_setting(config, "net", "port") == 8080);

  // The exception must arrive as a MissingKey, not as a sliced ConfigError.
  CHECK_THROWS_AS(read_setting(config, "net", "timeout"), MissingKey);

  try {
    read_setting(config, "net", "timeout");
    FAIL("expected a throw");
  } catch (const MissingKey& error) {
    CHECK(error.key() == "net.timeout");
    CHECK(std::string{error.what()} == "missing key: net.timeout");
  }
}

TEST_CASE("a bad value is reported as such") {
  const std::map<std::string, std::string> config = {{"net.port", "eighty"}};
  CHECK_THROWS_AS(read_setting(config, "net", "port"), BadValue);
}

TEST_CASE("every error is still catchable as the family, and as std::exception") {
  const std::map<std::string, std::string> config;
  CHECK_THROWS_AS(read_setting(config, "net", "port"), ConfigError);
  CHECK_THROWS_AS(read_setting(config, "net", "port"), std::exception);
}
