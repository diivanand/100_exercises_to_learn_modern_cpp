// Solution -- 03.07 std::unique_ptr
#include <doctest/doctest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
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

std::unique_ptr<Tool> make_tool(std::string name) {
  return std::make_unique<Tool>(std::move(name));
}

// Reads the tool, says nothing about ownership, works for a Tool anywhere.
std::string describe(const Tool& tool) {
  return "tool: " + tool.name();
}

class Workshop {
public:
  // Rule of zero: unique_ptr members give Workshop a correct destructor and
  // correct move operations without a line of code.
  void add(std::unique_ptr<Tool> tool) {
    tools_.push_back(std::move(tool));
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return tools_.size();
  }
  [[nodiscard]] const Tool& at(std::size_t index) const {
    return *tools_[index];
  }

  [[nodiscard]] std::unique_ptr<Tool> take(std::size_t index) {
    std::unique_ptr<Tool> tool = std::move(tools_[index]);
    tools_.erase(tools_.begin() + static_cast<std::ptrdiff_t>(index));
    return tool;
  }

private:
  std::vector<std::unique_ptr<Tool>> tools_;
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
