// =============================================================================
//  03.07 -- std::unique_ptr
// =============================================================================
//
//  `std::unique_ptr<T>` is the rule of five (03.05), already written, correct,
//  and free. It owns one object, it cannot be copied, it moves, and it deletes
//  what it holds when it goes out of scope. On every implementation that
//  matters it is the same size as a raw pointer, and dereferencing or
//  resetting it compiles to the same code. (Passing one *by value* is the one
//  place it costs a little: the non-trivial destructor forces it through
//  memory rather than a register on the common ABIs.)
//
//  Use it for:
//   * a member that must be heap-allocated (polymorphic types, pimpl,
//     large or optional objects);
//   * a factory return type: the caller is now responsible, and cannot forget;
//   * anything that used to be a `new` with a matching `delete` somewhere else.
//
//  Two things to know:
//
//   1. PREFER std::make_unique. `std::make_unique<T>(args...)` is one
//     allocation, exception-safe, and mentions T once instead of twice. Reserve
//     the `unique_ptr<T>{new T}` form for the rare case that needs a custom
//     deleter.
//
//   2. PASS IT ACCORDING TO OWNERSHIP (Core Guidelines R.30, F.7):
//
//        void f(const Widget& w)          // I just want to use it   <- default
//        void f(std::unique_ptr<W> w)     // I am taking ownership
//        void f(std::unique_ptr<W>& w)    // I may reseat your pointer
//
//     A function that only reads the object should take `const Widget&`. Taking
//     `const std::unique_ptr<Widget>&` says nothing about ownership and rules
//     out every caller who has the object on the stack.
//
//  TASK
//    Replace the raw owning pointers in `Workshop` with std::unique_ptr, and
//    fix the four function signatures below to say what they actually mean.
//
//  NOTE  This exercise starts as a compile error.
//
//  RUN IT
//    ./mcpp test 03_07
//
// =============================================================================

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

class Tool {
public:
  explicit Tool(std::string name) : name_(std::move(name)) {
    ++live_count_;
  }
  ~Tool() {
    --live_count_;
  }

  Tool(const Tool&) = delete;
  Tool& operator=(const Tool&) = delete;
  Tool(Tool&&) = delete;
  Tool& operator=(Tool&&) = delete;

  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }

  [[nodiscard]] static int live_count() noexcept {
    return live_count_;
  }

private:
  std::string name_;
  static inline int live_count_ = 0;
};

// TODO: return a std::unique_ptr<Tool> built with std::make_unique. A raw
// pointer return type does not say who deletes it, so nobody does.
Tool* make_tool(std::string name) {
  return new Tool{std::move(name)};
}

// TODO: this only reads the tool. Take a `const Tool&`.
std::string describe(const Tool* tool) {
  return "tool: " + tool->name();
}

class Workshop {
public:
  Workshop() = default;

  // TODO: `tools_` should be a std::vector<std::unique_ptr<Tool>>, and this
  // should take ownership: `void add(std::unique_ptr<Tool> tool)`.
  void add(Tool* tool) {
    tools_.push_back(tool);
  }

  // TODO: once tools_ holds unique_ptrs, this destructor becomes unnecessary.
  // Deleting it is the rule of zero (03.04) doing its job.
  ~Workshop() {
    for (Tool* tool : tools_) {
      delete tool;
    }
  }

  Workshop(const Workshop&) = delete;
  Workshop& operator=(const Workshop&) = delete;

  [[nodiscard]] std::size_t size() const noexcept {
    return tools_.size();
  }
  [[nodiscard]] const Tool& at(std::size_t index) const {
    return *tools_[index];
  }

  // Hands a tool back to the caller, who now owns it.
  // TODO: return std::unique_ptr<Tool>.
  Tool* take(std::size_t index) {
    Tool* tool = tools_[index];
    tools_.erase(tools_.begin() + static_cast<std::ptrdiff_t>(index));
    return tool;
  }

private:
  std::vector<Tool*> tools_;
};

TEST_CASE("make_tool hands over ownership") {
  CHECK(Tool::live_count() == 0);
  {
    const std::unique_ptr<Tool> hammer = make_tool("hammer");
    CHECK(Tool::live_count() == 1);
    CHECK(describe(*hammer) == "tool: hammer");
  }
  CHECK(Tool::live_count() == 0);
}

TEST_CASE("a Workshop owns its tools and cleans up") {
  CHECK(Tool::live_count() == 0);
  {
    Workshop workshop;
    workshop.add(make_tool("saw"));
    workshop.add(std::make_unique<Tool>("plane"));
    CHECK(workshop.size() == 2);
    CHECK(Tool::live_count() == 2);
    CHECK(workshop.at(1).name() == "plane");
  }
  CHECK(Tool::live_count() == 0);
}

TEST_CASE("take transfers ownership out again") {
  Workshop workshop;
  workshop.add(make_tool("chisel"));
  workshop.add(make_tool("file"));

  {
    const std::unique_ptr<Tool> mine = workshop.take(0);
    CHECK(mine->name() == "chisel");
    CHECK(workshop.size() == 1);
    CHECK(Tool::live_count() == 2);
  }
  CHECK(Tool::live_count() == 1);
}

TEST_CASE("a unique_ptr costs nothing extra") {
  static_assert(sizeof(std::unique_ptr<Tool>) == sizeof(Tool*));
  static_assert(!std::is_copy_constructible_v<std::unique_ptr<Tool>>);
  static_assert(std::is_nothrow_move_constructible_v<std::unique_ptr<Tool>>);
  CHECK(true);
}
