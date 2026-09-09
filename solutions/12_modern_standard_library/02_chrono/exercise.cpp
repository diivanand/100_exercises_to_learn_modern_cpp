// Solution -- 12.02 std::chrono
#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <thread>

using namespace std::chrono_literals;

std::string describe_timeout(std::chrono::milliseconds timeout) {
  return std::to_string(timeout.count()) + "ms";
}

std::chrono::milliseconds total(std::chrono::seconds a, std::chrono::milliseconds b) {
  // The common type of seconds and milliseconds is milliseconds -- the finer
  // one, because converting the other way could lose information.
  return a + b;
}

std::chrono::seconds whole_seconds(std::chrono::milliseconds duration) {
  return std::chrono::duration_cast<std::chrono::seconds>(duration);
}

template <typename F>
std::chrono::microseconds time_it(F&& work) {
  // steady_clock: monotonic by definition, so the difference cannot be
  // negative however the machine's wall clock behaves.
  const auto start = std::chrono::steady_clock::now();
  work();
  const auto finish = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(finish - start);
}

TEST_CASE("a duration carries its unit") {
  CHECK(describe_timeout(500ms) == "500ms");
  CHECK(describe_timeout(2s) == "2000ms");
  CHECK(describe_timeout(std::chrono::minutes{1}) == "60000ms");
}

TEST_CASE("arithmetic keeps the finer unit") {
  CHECK(total(1s, 500ms) == 1500ms);
  CHECK(total(0s, 1ms) == 1ms);
  CHECK(total(2s, 0ms) == 2000ms);
}

TEST_CASE("a lossy conversion has to be asked for") {
  CHECK(whole_seconds(1500ms) == 1s);
  CHECK(whole_seconds(999ms) == 0s);
  CHECK(whole_seconds(2000ms) == 2s);

  CHECK(std::chrono::round<std::chrono::seconds>(1500ms) == 2s);
  CHECK(std::chrono::floor<std::chrono::seconds>(1999ms) == 1s);
}

TEST_CASE("measuring uses a steady clock") {
  static_assert(std::chrono::steady_clock::is_steady);
  static_assert(!std::chrono::system_clock::is_steady);

  const auto elapsed = time_it([] { std::this_thread::sleep_for(20ms); });

  CHECK(elapsed.count() > 0);
  CHECK(elapsed >= 15ms);
}

TEST_CASE("durations compare and convert") {
  CHECK(1s == 1000ms);
  CHECK(1min > 59s);
  CHECK(90s == 1min + 30s);

  constexpr auto day = std::chrono::hours{24};
  static_assert(day == std::chrono::minutes{1440});
  CHECK(true);
}
