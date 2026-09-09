// =============================================================================
//  12.02 -- std::chrono
// =============================================================================
//
//  `<chrono>` makes time a TYPE rather than a number, so a function taking a
//  timeout cannot be passed a count of the wrong unit:
//
//      void wait(std::chrono::milliseconds timeout);
//      wait(500);                          // does not compile
//      wait(std::chrono::seconds{5});      // fine -- converted, exactly
//      wait(500ms);                        // with `using namespace
//                                          //   std::chrono_literals`
//
//  Conversions that cannot lose information are IMPLICIT (seconds to
//  milliseconds). Ones that can are not -- you ask for them with
//  `duration_cast`, and the cast truncates towards zero, which is why
//  `duration_cast<seconds>(1500ms)` is 1.
//
//  Three clocks, and they are not interchangeable:
//
//      steady_clock    never goes backwards. THE ONE FOR MEASURING DURATIONS.
//      system_clock    wall clock. Can jump when NTP corrects it. The one for
//                      timestamps, and the only one that maps to a calendar.
//      high_resolution_clock  an alias for one of the other two. Do not use
//                      it: you cannot tell which, so it may not be steady.
//
//  C++20 added the calendar and time zones: `year_month_day`, `sys_days`,
//  `weekday`, `2024y/March/15d`, and `std::format` support with strftime-like
//  specs. Library support varies; the arithmetic below is portable.
//
//  TASK
//    Fix the units, and use the right clock for each job.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 12_02
//
// =============================================================================

#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <thread>

using namespace std::chrono_literals;

// TODO: this takes a bare int, so a caller can pass 5 meaning either seconds
// or milliseconds and nothing will tell them which. Take a
// std::chrono::milliseconds instead.
std::string describe_timeout(int milliseconds) {
  return std::to_string(milliseconds) + "ms";
}

// TODO: return the total as milliseconds. Adding a seconds to a milliseconds
// gives a milliseconds, because that is the common type that loses nothing.
std::chrono::milliseconds total(std::chrono::seconds a, std::chrono::milliseconds b) {
  return {};
}

// TODO: truncate a duration to whole seconds. This one CAN lose information,
// so it needs an explicit duration_cast.
std::chrono::seconds whole_seconds(std::chrono::milliseconds duration) {
  return {};
}

// Measures how long `work` takes.
//
// TODO: use steady_clock. system_clock can jump backwards when the machine's
// clock is corrected, which makes a measured duration negative -- a real bug
// that appears once a month in production and never in testing.
template <typename F>
std::chrono::microseconds time_it(F&& work) {
  const auto start = std::chrono::system_clock::now();
  work();
  const auto finish = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(finish - start);
}

TEST_CASE("a duration carries its unit") {
  CHECK(describe_timeout(500ms) == "500ms");
  // Seconds convert to milliseconds implicitly: nothing is lost.
  CHECK(describe_timeout(2s) == "2000ms");
  CHECK(describe_timeout(std::chrono::minutes{1}) == "60000ms");

  // And this does not compile, which is the entire point:
  //
  //   describe_timeout(500);
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

  // duration_cast truncates towards zero rather than rounding. C++17 added
  // floor, ceil and round if you want something else.
  CHECK(std::chrono::round<std::chrono::seconds>(1500ms) == 2s);
  CHECK(std::chrono::floor<std::chrono::seconds>(1999ms) == 1s);
}

TEST_CASE("measuring uses a steady clock") {
  static_assert(std::chrono::steady_clock::is_steady);
  static_assert(!std::chrono::system_clock::is_steady);

  const auto elapsed = time_it([] { std::this_thread::sleep_for(20ms); });

  // A measured duration must never be negative. With system_clock that is not
  // a guarantee, only a probability.
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
