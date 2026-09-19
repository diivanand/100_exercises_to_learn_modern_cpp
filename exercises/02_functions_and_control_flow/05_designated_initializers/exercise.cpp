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
//  NOTE  On most compilers this exercise compiles and fails at run time: a
//        member left out of a designated initialiser is value-initialised
//        (zero, empty, false) when it has no default, which is rarely what
//        was meant. Clang 18 and newer add a warning for exactly this,
//        `-Wmissing-designated-field-initializers`, so there the starter is
//        a build error instead. Either way the fix is the same: give the
//        members defaults.
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
  std::string host = "localhost";
  int port = 80;
  std::chrono::seconds timeout = std::chrono::seconds{30};
  bool verify_tls = true;
  bool follow_redirects = true;
  int max_retries = 3;
};

// TODO: return a config for a local development server: host "127.0.0.1",
// port 8080, TLS verification off. Everything else stays at its default.
// Use designated initialisers.
HttpConfig development_config() {
  return HttpConfig{
    .host = "127.0.0.1",
    .port = 8080,
    .verify_tls = false,
  };
}

// TODO: return a config that keeps every default except the timeout, which
// becomes `timeout`.
HttpConfig with_timeout(std::chrono::seconds timeout) {
  (void)timeout;
  return HttpConfig{
    .timeout = timeout,
  };
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
