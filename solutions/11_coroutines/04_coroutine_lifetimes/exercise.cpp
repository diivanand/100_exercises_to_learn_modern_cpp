// Solution -- 11.04 Coroutine lifetimes
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

// A coroutine's parameters are copied INTO the frame, so a by-value parameter
// lives exactly as long as the coroutine does. A reference parameter copies
// the reference, and the referent is not extended by anything.
//
// Taking `prefix` and `values` by value is the fix, and it is why coroutine
// parameters are one of the few places where "take everything by value" is
// simply correct.
Generator<std::string> labelled(std::string prefix, std::vector<int> values) {
  for (const int value : values) {
    co_yield prefix + std::to_string(value);
  }
}

// A lambda that is a coroutine has the same problem, one level up: the closure
// object is not part of the coroutine frame, so captures die when the lambda
// temporary does.
//
// The fix here is to keep the lambda alive in a named variable for as long as
// the generator it produced.
Generator<int> scaled(int factor, std::vector<int> values) {
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
