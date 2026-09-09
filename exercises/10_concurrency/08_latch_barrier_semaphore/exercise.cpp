// =============================================================================
//  10.08 -- latch, barrier and semaphore (C++20)
// =============================================================================
//
//  Three coordination primitives that C++20 finally standardised. Each one
//  replaces a hand-rolled mutex-plus-condition-variable that was easy to get
//  wrong.
//
//    std::latch          a SINGLE-USE countdown. Threads `count_down()`;
//                        threads `wait()` until it reaches zero. Once it does,
//                        it stays there. Use it for "wait until N things have
//                        happened" -- start gates, fan-out/fan-in.
//
//    std::barrier        a REUSABLE rendezvous for a fixed group. Every thread
//                        calls `arrive_and_wait()`; none proceeds until all
//                        have arrived, then the barrier resets for the next
//                        phase. It can also run a COMPLETION FUNCTION once,
//                        between phases -- which is where the per-phase
//                        bookkeeping goes.
//
//    std::counting_semaphore<N>   a permit counter. `acquire()` takes one
//                        (blocking if there are none), `release()` gives one
//                        back. `std::binary_semaphore` is the N=1 alias.
//                        Use it to LIMIT CONCURRENCY -- at most K threads in
//                        a section -- which a mutex cannot express.
//
//  A semaphore is not a mutex: it has no owner, so one thread may acquire and
//  another release. That is a feature for signalling and a hazard for mutual
//  exclusion -- use a mutex when you mean a mutex.
//
//  TASK
//    Use each of the three for the job it is for.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 10_08
//
// =============================================================================

#include <doctest/doctest.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <latch>
#include <semaphore>
#include <thread>
#include <vector>

// Starts `count` workers that all begin their real work at the same moment.
//
// TODO: use a std::latch as a start gate: every worker waits on it, and the
// main thread counts it down once everything is set up. Return how many
// workers ran.
//
// (Without a gate, the first worker may finish before the last is even
// created -- which makes "measure the parallel section" meaningless.)
int run_together(int count, std::atomic<int>& peak_concurrency) {
  std::atomic<int> ran{0};
  std::atomic<int> in_flight{0};

  {
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<std::size_t>(count));
    for (int t = 0; t < count; ++t) {
      workers.emplace_back([&] {
        const int current = in_flight.fetch_add(1) + 1;
        int previous_peak = peak_concurrency.load();
        while (previous_peak < current &&
               !peak_concurrency.compare_exchange_weak(previous_peak, current)) {
        }
        ran.fetch_add(1);
        in_flight.fetch_sub(1);
      });
    }
  }
  return ran.load();
}

// Runs `rounds` phases with `workers` threads. Every thread adds its index to
// the shared total in each phase, and NO thread may start phase n+1 until all
// have finished phase n.
//
// TODO: use a std::barrier, with a completion function that records the total
// at the end of each phase.
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
    std::vector<std::jthread> threads;
    threads.reserve(static_cast<std::size_t>(workers));
    for (int t = 0; t < workers; ++t) {
      threads.emplace_back([&, t] {
        for (int round = 0; round < rounds; ++round) {
          running_total.fetch_add(t + 1);
        }
      });
    }
  }
  return totals;
}

// Runs `task_count` tasks but allows at most `limit` of them to be inside the
// critical section at once.
//
// TODO: use a std::counting_semaphore. A mutex would allow one; this needs K.
int run_with_limit(int task_count, int limit, std::atomic<int>& observed_peak) {
  std::atomic<int> completed{0};
  std::atomic<int> concurrent{0};

  {
    std::vector<std::jthread> threads;
    threads.reserve(static_cast<std::size_t>(task_count));
    for (int t = 0; t < task_count; ++t) {
      threads.emplace_back([&] {
        const int current = concurrent.fetch_add(1) + 1;
        int previous = observed_peak.load();
        while (previous < current &&
               !observed_peak.compare_exchange_weak(previous, current)) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
        concurrent.fetch_sub(1);
        completed.fetch_add(1);
      });
    }
  }
  return completed.load();
}

TEST_CASE("a latch releases every worker at once") {
  std::atomic<int> peak{0};
  CHECK(run_together(8, peak) == 8);

  // With a start gate, several workers really are inside at the same time.
  // Without one this is often 1, and the test is measuring nothing.
  CHECK(peak.load() > 1);
}

TEST_CASE("a barrier separates phases") {
  const auto totals = run_phases(4, 3);

  REQUIRE(totals.size() == 3);
  // Each phase adds 1 + 2 + 3 + 4 = 10, and no phase may overlap the next --
  // so the totals are exactly 10, 20, 30.
  CHECK(totals[0] == 10);
  CHECK(totals[1] == 20);
  CHECK(totals[2] == 30);
}

TEST_CASE("a semaphore limits concurrency") {
  std::atomic<int> peak{0};
  CHECK(run_with_limit(20, 3, peak) == 20);

  // At most three tasks inside at once -- and that is a hard guarantee, not a
  // statistical one.
  CHECK(peak.load() <= 3);
  CHECK(peak.load() >= 1);
}
