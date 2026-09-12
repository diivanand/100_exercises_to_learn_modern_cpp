// Solution -- 09.05 std::function, and what it costs
#include <doctest/doctest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// A template parameter: inlined, no allocation, no indirect call. The cost is
// that this definition has to be visible to every caller.
template <typename Predicate>
int count_matching(const std::vector<int>& values, Predicate predicate) {
  int count = 0;
  for (const int value : values) {
    if (predicate(value)) {
      ++count;
    }
  }
  return count;
}

class EventBus {
public:
  // std::function earns its keep here: the handlers have different types and
  // have to live in one container, and they outlive the call that registered
  // them.
  void subscribe(std::function<void(const std::string&)> handler) {
    handlers_.push_back(std::move(handler));
  }

  void publish(const std::string& message) const {
    for (const auto& handler : handlers_) {
      handler(message);
    }
  }

  std::size_t handler_count() const noexcept {
    return handlers_.size();
  }

private:
  std::vector<std::function<void(const std::string&)>> handlers_;
};

// A minimal move-only equivalent of std::function<void()>. This is the same
// type-erasure shape as 04.09's Drawable, minus the clone -- which is exactly
// what makes it move-only, and what makes it able to hold a lambda that
// captured a unique_ptr.
//
// (C++23's std::move_only_function is this, done properly.)
class Task {
public:
  template <typename F>
  explicit Task(F&& action)
      : action_(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(action))) {}

  void run() const {
    action_->run();
  }

private:
  struct Concept {
    virtual ~Concept() = default;
    Concept() = default;
    Concept(const Concept&) = delete;
    Concept& operator=(const Concept&) = delete;
    virtual void run() const = 0;
  };

  template <typename F>
  struct Model final : Concept {
    explicit Model(F callable) : action(std::move(callable)) {}
    void run() const override {
      action();
    }
    mutable F action;
  };

  std::unique_ptr<Concept> action_;
};

TEST_CASE("a template parameter costs nothing") {
  const std::vector<int> values = {1, -2, 3, -4, 5};

  CHECK(count_matching(values, [](int n) { return n > 0; }) == 3);
  CHECK(count_matching(values, [](int n) { return n < 0; }) == 2);

  CHECK(count_matching(values, +[](int n) { return n == 3; }) == 1);
}

TEST_CASE("std::function is right when the callables must be stored together") {
  EventBus bus;
  std::vector<std::string> seen;
  int count = 0;

  bus.subscribe([&seen](const std::string& message) { seen.push_back(message); });
  bus.subscribe([&count](const std::string&) { ++count; });

  CHECK(bus.handler_count() == 2);

  bus.publish("hello");
  bus.publish("world");

  CHECK(seen == std::vector<std::string>{"hello", "world"});
  CHECK(count == 2);
}

TEST_CASE("a move-only callable does not fit in a std::function") {
  auto resource = std::make_unique<int>(42);
  int result = 0;

  Task task{[resource = std::move(resource), &result] { result = *resource; }};

  task.run();
  CHECK(result == 42);
}
