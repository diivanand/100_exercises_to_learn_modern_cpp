// =============================================================================
//  00.01 -- Hello, tests
// =============================================================================
//
//  Every exercise in this course is a single self-contained file that both
//  states the problem and checks your answer. There is no `main`: the test
//  runner supplies one. You edit the code above the tests, and you run the
//  tests to find out whether you were right.
//
//  TASK
//    Make `greet` return "Hello, " followed by the name and an exclamation
//    mark, so that `greet("world")` is "Hello, world!".
//
//  RUN IT
//    ./mcpp test 00_01
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <string_view>

// `std::string_view` is a non-owning view over characters. It is the right
// parameter type for a function that only *reads* a string and does not need
// to keep it: it binds to std::string, to string literals and to character
// buffers without copying any of them.
//
// See C++ Core Guidelines F.16 -- "For 'in' parameters, pass cheaply-copied
// types by value and others by reference to const".
std::string greet(std::string_view name) {
  // TODO: build the greeting.
  //
  // Note that you cannot write `"Hello, " + name`: a string literal is a
  // `const char[8]`, and there is no `operator+` for two raw character
  // sequences. Start from a `std::string` instead, then append.
  return std::string{name};
}

TEST_CASE("greet builds a greeting") {
  CHECK(greet("world") == "Hello, world!");
  CHECK(greet("modern C++") == "Hello, modern C++!");
}

TEST_CASE("greet accepts anything string-like") {
  const std::string owned = "Diivanand";
  CHECK(greet(owned) == "Hello, Diivanand!");

  // A view is cheap to make and cheap to pass -- no allocation happens here.
  constexpr std::string_view view = "view";
  CHECK(greet(view) == "Hello, view!");
}
