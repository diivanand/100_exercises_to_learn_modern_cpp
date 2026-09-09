// Solution -- 02.05 Designated initialisers
#include <doctest/doctest.h>

#include <chrono>
#include <string>

struct HttpConfig {
  std::string host = "localhost";
  int port = 80;
  std::chrono::seconds timeout{30};
  bool verify_tls = true;
  bool follow_redirects = true;
  int max_retries = 3;
};

HttpConfig development_config() {
  return HttpConfig{
      .host = "127.0.0.1",
      .port = 8080,
      .verify_tls = false,
  };
}

HttpConfig with_timeout(std::chrono::seconds timeout) {
  return HttpConfig{.timeout = timeout};
}

TEST_CASE("defaults are sensible on their own") {
  const HttpConfig config{};
  CHECK(config.host == "localhost");
  CHECK(config.port == 80);
  CHECK(config.timeout == std::chrono::seconds{30});
  CHECK(config.verify_tls);
  CHECK(config.follow_redirects);
  CHECK(config.max_retries == 3);
}

TEST_CASE("a partial config fills in the rest") {
  const HttpConfig config = development_config();
  CHECK(config.host == "127.0.0.1");
  CHECK(config.port == 8080);
  CHECK_FALSE(config.verify_tls);
  CHECK(config.timeout == std::chrono::seconds{30});
  CHECK(config.max_retries == 3);
}

TEST_CASE("changing one field does not disturb the others") {
  const HttpConfig config = with_timeout(std::chrono::seconds{5});
  CHECK(config.timeout == std::chrono::seconds{5});
  CHECK(config.host == "localhost");
  CHECK(config.verify_tls);
}

TEST_CASE("names survive a refactor that positions would not") {
  const HttpConfig ok{.port = 8080, .timeout = std::chrono::seconds{1}};
  CHECK(ok.port == 8080);
}
