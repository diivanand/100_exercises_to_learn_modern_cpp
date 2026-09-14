// =============================================================================
//  01.05 -- consteval and constinit (C++20)
// =============================================================================
//
//  C++20 splits `constexpr` into three keywords that each say one thing.
//
//    constexpr fn   "MAY run at compile time." If you call it with runtime
//                   arguments it silently becomes an ordinary function -- so
//                   a refactor can quietly move work back into the runtime
//                   without anyone noticing.
//
//    consteval fn   "MUST run at compile time." An *immediate function*.
//                   Calling it with a runtime argument is a compile error.
//                   Use it when compile-time evaluation is the whole point:
//                   validating a format string, hashing an identifier,
//                   building a lookup table.
//
//    constinit var  "MUST be initialised at compile time" -- but the variable
//                   is not const afterwards. This is the cure for the static
//                   initialisation order fiasco: a `constinit` global is
//                   filled in before any dynamic initialisation runs, so no
//                   other translation unit can observe it half-built.
//
//  TASK
//    Make `checksum` an immediate function, make `kGeneration` constinit, and
//    make `parse_version` reject anything that is not a well-formed version at
//    *compile* time.
//
//
//  NOTE  This exercise starts as a compile error -- the static_asserts below
//        describe behaviour parse_version does not have yet.
//
//  RUN IT
//    ./mcpp test 01_05
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <string_view>

// A tiny FNV-1a hash. Hashing a string literal is exactly the kind of work
// that should never survive into the binary as instructions.
//
// TODO: make this `consteval` so that calling it with a runtime string is a
// compile error rather than a silent cost.
consteval std::uint32_t checksum(std::string_view text) {
  std::uint32_t hash = 2166136261U;
  for (const char c : text) {
    hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(c));
    hash *= 16777619U;
  }
  return hash;
}

// A mutable global counter. We want its *initial* value baked into the binary,
// with no run-time initialisation step, while still being able to bump it.
//
// TODO: mark it `constinit`. (Note that `constexpr` would not work here: the
// variable is modified below, and constexpr implies const.)
constinit std::uint32_t kGeneration = checksum("v1");

struct Version {
  int major = 0;
  int minor = 0;

  friend constexpr bool operator==(const Version&, const Version&) = default;
};

consteval bool is_digit(char c) { return c >= '0' && c <= '9'; }

// Parses "MAJOR.MINOR" for single-digit components.
//
// TODO: implement it, and make it `consteval`.
//
// Then handle malformed input by *throwing*. That sounds odd for a function
// that never runs at run time, and that is exactly the trick: a `throw` cannot
// appear in a constant expression, so reaching one turns the call into a
// compile error at the call site. It is how you write a precondition the
// compiler enforces -- with a readable message attached.
consteval Version parse_version(std::string_view text) {
  if (text.size() != 3
    || !is_digit(text[0])
    || text[1] != '.'
    || !is_digit(text[2])) {
    throw std::invalid_argument{"invalid version string"};
  }
  return Version{.major = text[0] - '0', .minor = text[2] - '0'};
}

TEST_CASE("checksum runs at compile time") {
  static_assert(checksum("v1") == checksum("v1"));
  static_assert(checksum("v1") != checksum("v2"));

  // With `consteval` in place, this line would not compile -- which is the
  // point. Uncomment it to see the error message:
  //
  //   std::string runtime_text = "v1";
  //   (void)checksum(runtime_text);

  CHECK(checksum("v1") != 0);
}

TEST_CASE("kGeneration is initialised before the program starts, but mutable") {
  const std::uint32_t original = kGeneration;
  kGeneration += 1;
  CHECK(kGeneration == original + 1);
  kGeneration = original;
}

TEST_CASE("parse_version validates at compile time") {
  static_assert(parse_version("1.4") == Version{1, 4});
  static_assert(parse_version("0.9") == Version{0, 9});

  // Each of these should fail to *compile* once parse_version throws on bad
  // input. Uncomment one at a time to check your work:
  //
  //   constexpr auto bad_shape = parse_version("1-4");
  //   constexpr auto too_long  = parse_version("1.4.7");
  //   constexpr auto not_digit = parse_version("a.b");

  CHECK(parse_version("2.0").major == 2);
}
