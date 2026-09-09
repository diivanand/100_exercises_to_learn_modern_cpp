// Solution -- 03.02 lvalues, rvalues, and T&&
#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

struct Sink {
  std::vector<std::string> values;
  int copies = 0;
  int moves = 0;

  void store(const std::string& value) {
    values.push_back(value);
    ++copies;
  }

  void store(std::string&& value) {
    // `value` names an object, so it is an lvalue here. Without std::move this
    // would select the const& overload and copy.
    values.push_back(std::move(value));
    ++moves;
  }
};

void store_twice(Sink& sink, std::string value) {
  sink.store(value);
  sink.store(std::move(value));
}

TEST_CASE("overload resolution picks by value category") {
  Sink sink;

  const std::string named = "lvalue";
  sink.store(named);
  CHECK(sink.copies == 1);
  CHECK(sink.moves == 0);

  sink.store("temporary");
  CHECK(sink.copies == 1);
  CHECK(sink.moves == 1);

  std::string give_away = "cast";
  sink.store(std::move(give_away));
  CHECK(sink.moves == 2);

  CHECK(sink.values == std::vector<std::string>{"lvalue", "temporary", "cast"});
}

TEST_CASE("a moved-from std::string is valid but unspecified") {
  std::string source = "some text long enough to own a heap buffer";
  const std::string target = std::move(source);

  CHECK(target == "some text long enough to own a heap buffer");

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
