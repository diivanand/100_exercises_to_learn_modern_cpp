// =============================================================================
//  02.05 -- Designated initialisers (C++20)
// =============================================================================
//
//  Aggregate initialisation by position is a bug waiting for a refactor:
//
//      HttpConfig config{"localhost", 8080, 30, true, false};
//
//  Which of those is `follow_redirects`? What happens when someone inserts a
//  field in the middle? Both questions have the same answer: nobody knows
//  until it breaks.
//
//  C++20 lets you name the members:
//
//      HttpConfig config{
//          .host = "localhost",
//          .port = 8080,
//          .verify_tls = true,
//      };
//
//  Rules worth knowing, because they are stricter than C's version:
//
//   * the initialisers must appear in DECLARATION ORDER -- you may skip
//     members, but you may not reorder them;
//   * skipped members get their default member initialiser, or are
//     value-initialised if they have none;
//   * this works for aggregates only: no user-declared constructors, no
//     private non-static data, no virtual bases.
//
//  Which is why "give every member a default" is such a cheap habit: it turns
//  a partially-specified struct into a valid one.
//
//  TASK
//    Give `HttpConfig` sensible defaults, then build the configurations the
//    tests describe using designated initialisers.
//
//  RUN IT
//    ./mcpp test 02_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <chrono>
#include <string>

struct HttpConfig {
  // TODO: give every member a default member initialiser, so that a caller can
  // specify only the fields they care about. Sensible values:
  //   host "localhost", port 80, timeout 30s, verify_tls true,
  //   follow_redirects true, max_retries 3.
  std::string host;
  int port;
  std::chrono::seconds timeout;
  bool verify_tls;
  bool follow_redirects;
  int max_retries;
};

// TODO: return a config for a local development server: host "127.0.0.1",
// port 8080, TLS verification off. Everything else stays at its default.
// Use designated initialisers.
HttpConfig development_config() {
  return HttpConfig{};
}

// TODO: return a config that keeps every default except the timeout, which
// becomes `timeout`.
HttpConfig with_timeout(std::chrono::seconds timeout) {
  (void)timeout;
  return HttpConfig{};
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
  // Untouched fields keep their defaults.
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
  // Skipping members is allowed; reordering them is not. Uncommenting this
  // should fail to compile, because `port` is declared before `timeout`:
  //
  //   const HttpConfig wrong{.timeout = std::chrono::seconds{1}, .port = 8080};
  const HttpConfig ok{.port = 8080, .timeout = std::chrono::seconds{1}};
  CHECK(ok.port == 8080);
}
