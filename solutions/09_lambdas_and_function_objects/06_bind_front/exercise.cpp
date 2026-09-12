// Solution -- 09.06 std::bind_front and std::invoke
#include <doctest/doctest.h>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

struct Logger {
  std::string prefix;
  std::vector<std::string> lines;

  void log(const std::string& level, const std::string& message) {
    lines.push_back(prefix + " [" + level + "] " + message);
  }

  std::size_t count() const {
    return lines.size();
  }
};

int scaled_sum(int factor, int a, int b) {
  return factor * (a + b);
}

int add(int a, int b) {
  return a + b;
}

auto adder(int amount) {
  return std::bind_front(add, amount);
}

auto info_logger(Logger& logger) {
  // For a pointer to member function, the first bound argument is the object.
  // `std::ref` keeps it a reference -- otherwise the Logger would be copied
  // into the callable and the caller's lines would never change.
  return std::bind_front(&Logger::log, std::ref(logger), "info");
}

template <typename F, typename... Args>
decltype(auto) call(F&& f, Args&&... arguments) {
  return std::invoke(std::forward<F>(f), std::forward<Args>(arguments)...);
}

TEST_CASE("bind_front fixes the leading arguments") {
  const auto add5 = adder(5);
  CHECK(add5(1) == 6);
  CHECK(add5(-5) == 0);

  const auto triple_sum = std::bind_front(scaled_sum, 3);
  CHECK(triple_sum(1, 2) == 9);
}

TEST_CASE("bind_front over a member function") {
  Logger logger{"app", {}};
  const auto info = info_logger(logger);

  info("started");
  info("ready");

  CHECK(logger.count() == 2);
  CHECK(logger.lines[0] == "app [info] started");
  CHECK(logger.lines[1] == "app [info] ready");
}

TEST_CASE("invoke calls anything the same way") {
  Logger logger{"svc", {}};

  CHECK(call(add, 2, 3) == 5);
  CHECK(call([](int n) { return n * 10; }, 4) == 40);

  call(&Logger::log, logger, "warn", "disk full");
  CHECK(logger.lines.at(0) == "svc [warn] disk full");

  CHECK(call(&Logger::prefix, logger) == "svc");
}

TEST_CASE("a lambda is still often the clearest option") {
  const auto reversed_subtract = [](int a, int b) { return b - a; };
  CHECK(reversed_subtract(3, 10) == 7);
}
