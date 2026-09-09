// Solution -- 01.05 consteval and constinit
#include <doctest/doctest.h>

#include <cstdint>
#include <string_view>

consteval std::uint32_t checksum(std::string_view text) {
  std::uint32_t hash = 2166136261U;
  for (const char c : text) {
    hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(c));
    hash *= 16777619U;
  }
  return hash;
}

constinit std::uint32_t kGeneration = checksum("v1");

struct Version {
  int major = 0;
  int minor = 0;

  friend constexpr bool operator==(const Version&, const Version&) = default;
};

consteval Version parse_version(std::string_view text) {
  // In an immediate function a throw that is actually reached makes the call
  // a non-constant expression, which the compiler reports at the call site.
  // This is the standard "compile-time precondition" idiom in C++20.
  if (text.size() != 3 || text[1] != '.') {
    throw "version must have the form MAJOR.MINOR with single digits";
  }
  if (text[0] < '0' || text[0] > '9' || text[2] < '0' || text[2] > '9') {
    throw "version components must be digits";
  }
  return Version{text[0] - '0', text[2] - '0'};
}

TEST_CASE("checksum runs at compile time") {
  static_assert(checksum("v1") == checksum("v1"));
  static_assert(checksum("v1") != checksum("v2"));
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
  CHECK(parse_version("2.0").major == 2);
}
