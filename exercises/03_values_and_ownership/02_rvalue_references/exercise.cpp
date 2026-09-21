// =============================================================================
//  03.02 -- lvalues, rvalues, and T&&
// =============================================================================
//
//  Every expression in C++ has a value category as well as a type. The two you
//  need day to day:
//
//    lvalue   has an identity you can take the address of.
//             `x`, `v[0]`, `*p`, a function returning `T&`.
//
//    rvalue   a temporary, or something you have explicitly said you are
//             finished with. Its resources can be stolen safely.
//             `42`, `x + y`, a function returning `T`, `std::move(x)`.
//
//  (The standard splits rvalues into prvalues and xvalues. That distinction
//  matters when you write library code; it does not change the rule below.)
//
//  `T&&` is a reference that binds ONLY to rvalues, which is how a function
//  can know it is safe to gut its argument:
//
//      void take(const std::string& s);   // I will read it
//      void take(std::string&& s);        // I may empty it
//
//  Two things that surprise everyone:
//
//   1. `std::move` does not move anything. It is a cast to `T&&` -- a way of
//      saying "treat this lvalue as an rvalue". The moving is done by whatever
//      constructor or assignment operator the cast then selects.
//
//   2. Inside `void take(std::string&& s)`, the parameter `s` is an LVALUE. It
//      has a name, so you can take its address. Passing it on without another
//      `std::move` will copy.
//
//  TASK
//    Implement the two overloads of `store` so the tests can tell them apart,
//    then fix `store_twice`, which currently copies when it should move.
//
//  RUN IT
//    ./mcpp test 03_02
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

// Records how a value arrived. In real code you would not need both overloads
// -- pass by value and move (02.06) -- but writing them out once makes the
// binding rules concrete.
struct Sink {
  std::vector<std::string> values;
  int copies = 0;
  int moves = 0;

  // TODO: append a copy of `value` and increment `copies`.
  void store(const std::string& value) {
    values.push_back(value);
    copies++;
  }

  // TODO: append `value` *by moving it* and increment `moves`. Remember that
  // `value` is an lvalue inside this function, even though its type is
  // `std::string&&`.
  void store(std::string&& value) {
    values.push_back(std::move(value));
    moves++;
  }
};

// Stores `value` twice: once by copying (the first store still needs it) and
// once by moving (nothing reads it afterwards).
//
// TODO: the second call should move. Adding `std::move` on the *last* use of
// a variable is the whole technique.
void store_twice(Sink& sink, std::string value) {
  sink.store(value);
  sink.store(std::move(value));
}

TEST_CASE("overload resolution picks by value category") {
  Sink sink;

  const std::string named = "lvalue";
  sink.store(named); // lvalue -> const&
  CHECK(sink.copies == 1);
  CHECK(sink.moves == 0);

  sink.store("temporary"); // rvalue -> &&
  CHECK(sink.copies == 1);
  CHECK(sink.moves == 1);

  std::string give_away = "cast";
  sink.store(std::move(give_away)); // explicit cast to rvalue -> &&
  CHECK(sink.moves == 2);

  CHECK(sink.values == std::vector<std::string>{"lvalue", "temporary", "cast"});
}

TEST_CASE("a moved-from std::string is valid but unspecified") {
  std::string source = "some text long enough to own a heap buffer";
  const std::string target = std::move(source);

  CHECK(target == "some text long enough to own a heap buffer");

  // The standard says `source` is in a valid but unspecified state. You may
  // assign to it or destroy it; you may not assume what it contains. Asserting
  // it is empty would be testing your library, not your code.
  source = "reused";
  CHECK(source == "reused");
}

TEST_CASE("store_twice copies once and moves once") {
  Sink sink;
  store_twice(sink, "payload");
  CHECK(sink.copies == 1);
  CHECK(sink.moves == 1);
  CHECK(sink.values.size() == 2);
  CHECK(sink.values[0] == "payload");
}
