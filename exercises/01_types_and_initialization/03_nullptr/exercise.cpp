// =============================================================================
//  01.03 -- nullptr, and why NULL was never a pointer
// =============================================================================
//
//  In C, `NULL` is a macro for `0` (or `((void*)0)`). In C++ before C++11 it
//  had to be an integer literal, which means this happens:
//
//      void log(int level);
//      void log(const char* message);
//
//      log(NULL);   // calls log(int). Almost certainly not what you meant.
//
//  `nullptr` is a real value of a real type, `std::nullptr_t`. It converts to
//  any pointer type and to nothing else, so overload resolution picks the
//  pointer. Core Guidelines ES.47: "Use nullptr rather than 0 or NULL".
//
//  It also reads better at a glance: `if (p == nullptr)` says "this is a
//  pointer" in a way `if (p == 0)` does not.
//
//  TASK
//    `describe` should report which overload was selected. Fill in the
//    `call_with_null` function so it reaches the *pointer* overload, and fix
//    `first_char` so it handles a null pointer instead of dereferencing it.
//
//  RUN IT
//    ./mcpp test 01_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <string>

std::string describe(int) {
  return "int";
}
std::string describe(const char*) {
  return "pointer";
}
std::string describe(std::nullptr_t) {
  return "nullptr_t";
}

// TODO: return `describe(...)` called with a null pointer constant such that
// the `const char*` overload runs. Note that passing `nullptr` directly picks
// the more specialised `std::nullptr_t` overload -- you need a value that is
// *typed* as a pointer.
std::string call_with_null() {
  return describe(static_cast<const char*>(nullptr));
}

// Returns the first character of a C string, or nothing if there is no string
// and nothing if the string is empty.
std::optional<char> first_char(const char* text) {
  // TODO: `text` may be null. Compare it against nullptr before dereferencing;
  // reading through a null pointer is undefined behaviour, not a crash you can
  // rely on.
  if (text == nullptr || std::string_view(text).empty()) {
    return std::nullopt;
  }
  return text[0];
}

TEST_CASE("overload resolution distinguishes 0, nullptr and a null pointer") {
  CHECK(describe(0) == "int");
  CHECK(describe(nullptr) == "nullptr_t");
  CHECK(call_with_null() == "pointer");
}

TEST_CASE("first_char guards against null") {
  CHECK(first_char("abc") == 'a');
  CHECK_FALSE(first_char("").has_value());
  CHECK_FALSE(first_char(nullptr).has_value());
}
