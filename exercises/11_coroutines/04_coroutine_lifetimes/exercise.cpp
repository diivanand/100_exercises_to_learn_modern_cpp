// =============================================================================
//  11.04 -- Coroutine lifetimes
// =============================================================================
//
//  Coroutines have their own family of dangling-reference bugs, and they are
//  worse than the usual ones because the code looks obviously fine.
//
//  THE RULE: a coroutine's parameters are copied into the coroutine frame, and
//  live as long as the frame. A REFERENCE parameter copies the reference --
//  and nothing extends the lifetime of what it refers to.
//
//      Generator<int> bad(const std::vector<int>& values) {   // DANGEROUS
//        for (int v : values) { co_yield v; }
//      }
//      auto g = bad({1, 2, 3});   // the temporary died at the semicolon;
//                                 // the coroutine has not run a line yet
//
//  A normal function would have consumed the argument before the temporary
//  died. A lazy coroutine has not started, so by the time it looks at
//  `values`, there is nothing there.
//
//  So: TAKE COROUTINE PARAMETERS BY VALUE. It is one of the very few places
//  where "by value, always" is simply the right answer, and it is why the
//  functions below have the signatures they do.
//
//  Two more:
//
//   * A LAMBDA THAT IS A COROUTINE has the same problem one level up. The
//     closure object is not part of the frame, so captures die with the
//     lambda temporary. Name the lambda, or capture by value into the
//     coroutine's own parameters.
//
//   * DESTROYING AN UNFINISHED FRAME still runs the destructors of everything
//     alive in it. That is what makes abandoning a generator safe -- and it is
//     why the owning type must call `destroy()`.
//
//  TASK
//    Fix the two signatures, then predict what the last two tests will say
//    before you run them.
//
//  NOTE  This exercise compiles as it stands. Its bugs are use-after-frees,
//        which may or may not show up as wrong output -- run it under
//        `cmake --preset asan` to see them for certain.
//
//  RUN IT
//    ./mcpp test 11_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <coroutine>
#include <exception>
#include <string>
#include <utility>
#include <vector>

template <typename T>
class Generator {
public:
  struct promise_type {
    T current_value{};

    Generator get_return_object() {
      return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept {
      return {};
    }
    std::suspend_always final_suspend() noexcept {
      return {};
    }
    std::suspend_always yield_value(T value) {
      current_value = std::move(value);
      return {};
    }
    void return_void() {}
    void unhandled_exception() {
      std::terminate();
    }
  };

  explicit Generator(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
  Generator(const Generator&) = delete;
  Generator& operator=(const Generator&) = delete;
  Generator(Generator&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
  Generator& operator=(Generator&& other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }
  ~Generator() {
    if (handle_) {
      handle_.destroy();
    }
  }

  bool next() {
    handle_.resume();
    return !handle_.done();
  }

  const T& value() const {
    return handle_.promise().current_value;
  }

private:
  std::coroutine_handle<promise_type> handle_{};
};

// TODO: both parameters are references to temporaries that have already been
// destroyed by the time this coroutine first runs. Take them BY VALUE so they
// are copied into the frame.
Generator<std::string> labelled(const std::string& prefix,
                                const std::vector<int>& values) {
  for (const int value : values) {
    co_yield prefix + std::to_string(value);
  }
}

// TODO: same fix. `values` must be owned by the frame, not referred to.
Generator<int> scaled(int factor, const std::vector<int>& values) {
  for (const int value : values) {
    co_yield value* factor;
  }
}

int live_frames = 0;

struct FrameMarker {
  FrameMarker() {
    ++live_frames;
  }
  ~FrameMarker() {
    --live_frames;
  }
  FrameMarker(const FrameMarker&) = delete;
  FrameMarker& operator=(const FrameMarker&) = delete;
};

Generator<int> counting(int limit) {
  const FrameMarker marker;
  for (int i = 0; i < limit; ++i) {
    co_yield i;
  }
}

TEST_CASE("by-value parameters live in the frame") {
  Generator<std::string> generator = [] {
    // Both arguments are temporaries that die at the end of this statement.
    // Because the parameters are by value, the coroutine copied them into its
    // own frame first.
    return labelled(std::string{"item-"}, std::vector<int>{1, 2, 3});
  }();

  // Churn the stack and the heap, so that a reference into a destroyed
  // temporary cannot quietly survive and make a broken version look correct.
  std::vector<std::string> noise;
  noise.reserve(64);
  for (int i = 0; i < 64; ++i) {
    noise.emplace_back(64, 'x');
  }

  std::vector<std::string> seen;
  while (generator.next()) {
    seen.push_back(generator.value());
  }

  CHECK(seen == std::vector<std::string>{"item-1", "item-2", "item-3"});
}

TEST_CASE("a generator over a temporary container") {
  auto generator = scaled(10, {1, 2, 3});

  std::vector<std::vector<int>> noise;
  noise.reserve(64);
  for (int i = 0; i < 64; ++i) {
    noise.emplace_back(16, i);
  }

  std::vector<int> seen;
  while (generator.next()) {
    seen.push_back(generator.value());
  }
  CHECK(seen == std::vector<int>{10, 20, 30});
}

TEST_CASE("abandoning a coroutine still runs its destructors") {
  CHECK(live_frames == 0);
  {
    auto generator = counting(1000);
    generator.next();
    generator.next();
    CHECK(live_frames == 1);
    // The generator's destructor destroys the frame, which destroys `marker`
    // even though the coroutine never reached the end of its body.
  }
  CHECK(live_frames == 0);
}

TEST_CASE("a completed coroutine has already cleaned up") {
  CHECK(live_frames == 0);
  {
    auto generator = counting(2);
    while (generator.next()) {
    }
    // The body has run to the end, so `marker` is already gone -- but the
    // frame itself lives until the Generator is destroyed, because
    // final_suspend is suspend_always.
    CHECK(live_frames == 0);
  }
  CHECK(live_frames == 0);
}
