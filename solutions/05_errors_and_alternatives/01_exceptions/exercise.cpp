// Solution -- 05.01 Exceptions
#include <doctest/doctest.h>

#include <map>
#include <stdexcept>
#include <string>

class ConfigError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class MissingKey : public ConfigError {
public:
  explicit MissingKey(const std::string& key)
      : ConfigError{"missing key: " + key}, key_{key} {}

  [[nodiscard]] const std::string& key() const noexcept {
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

int read_setting(const std::map<std::string, std::string>& config,
                 const std::string& section, const std::string& key) {
  try {
    return require_int(config, section + "." + key);
  } catch (const ConfigError&) {
    // Bare `throw;` rethrows the original object with its original type. The
    // handler could log here first without disturbing what the caller sees.
    throw;
  }
}

TEST_CASE("a missing key keeps its type and its detail") {
  const std::map<std::string, std::string> config = {{"net.port", "8080"}};

  CHECK(read_setting(config, "net", "port") == 8080);

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
