// Solution -- 10.08 latch, barrier and semaphore
#include <doctest/doctest.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <latch>
#include <semaphore>
#include <thread>
#include <vector>

int run_together(int count, std::atomic<int>& peak_concurrency) {
  std::atomic<int> ran{0};
  std::atomic<int> in_flight{0};

  // A single-use gate: every worker blocks on it until the main thread counts
  // it down, so they all enter the measured section together.
  std::latch start_gate{1};

  {
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<std::size_t>(count));
    for (int t = 0; t < count; ++t) {
      workers.emplace_back([&] {
        start_gate.wait();

        const int current = in_flight.fetch_add(1) + 1;
        int previous_peak = peak_concurrency.load();
        while (previous_peak < current &&
               !peak_concurrency.compare_exchange_weak(previous_peak, current)) {
        }

        // Hold the section long enough for the overlap to be observable.
        std::this_thread::sleep_for(std::chrono::milliseconds{5});

        ran.fetch_add(1);
        in_flight.fetch_sub(1);
      });
    }

    start_gate.count_down();
  }
  return ran.load();
}

std::vector<long> run_phases(int workers, int rounds) {
  // Sized up front, and written by index below. A barrier's completion
  // function must be noexcept, and push_back can allocate -- so the promise is
  // only keepable if there is nothing left in it that can throw. Declaring
  // noexcept over code that can throw does not make it safe; it just turns a
  // std::bad_alloc into a std::terminate (02.04).
  std::vector<long> totals(static_cast<std::size_t>(rounds), 0);
  std::size_t phase = 0;
  std::atomic<long> running_total{0};

  {
    // The completion function runs exactly once per phase, on one of the
    // arriving threads, after all have arrived and before any is released --
    // so it can read the phase's result with no lock.
    std::barrier phase_end{workers, [&totals, &running_total, &phase]() noexcept {
                             totals[phase++] = running_total.load();
                           }};

    std::vector<std::jthread> threads;
    threads.reserve(static_cast<std::size_t>(workers));
    for (int t = 0; t < workers; ++t) {
      threads.emplace_back([&, t] {
        for (int round = 0; round < rounds; ++round) {
          running_total.fetch_add(t + 1);
          phase_end.arrive_and_wait();
        }
      });
    }
  }
  return totals;
}

int run_with_limit(int task_count, int limit, std::atomic<int>& observed_peak) {
  std::atomic<int> completed{0};
  std::atomic<int> concurrent{0};

  // The template argument is the maximum the semaphore can ever hold; the
  // constructor argument is how many permits it starts with.
  std::counting_semaphore<64> permits{limit};

  {
    std::vector<std::jthread> threads;
    threads.reserve(static_cast<std::size_t>(task_count));
    for (int t = 0; t < task_count; ++t) {
      threads.emplace_back([&] {
        permits.acquire();

        const int current = concurrent.fetch_add(1) + 1;
        int previous = observed_peak.load();
        while (previous < current &&
               !observed_peak.compare_exchange_weak(previous, current)) {
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{1});

        concurrent.fetch_sub(1);
        completed.fetch_add(1);
        permits.release();
      });
    }
  }
  return completed.load();
}

TEST_CASE("a latch releases every worker at once") {
  std::atomic<int> peak{0};
  CHECK(run_together(8, peak) == 8);
  CHECK(peak.load() > 1);
}

TEST_CASE("a barrier separates phases") {
  const auto totals = run_phases(4, 3);

  REQUIRE(totals.size() == 3);
  CHECK(totals[0] == 10);
  CHECK(totals[1] == 20);
  CHECK(totals[2] == 30);
}

TEST_CASE("a semaphore limits concurrency") {
  std::atomic<int> peak{0};
  CHECK(run_with_limit(20, 3, peak) == 20);

  CHECK(peak.load() <= 3);
  CHECK(peak.load() >= 1);
}
