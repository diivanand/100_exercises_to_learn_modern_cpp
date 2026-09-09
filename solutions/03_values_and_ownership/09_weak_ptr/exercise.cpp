// Solution -- 03.09 std::weak_ptr
#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

struct Node {
  explicit Node(std::string name) : name(std::move(name)) {
    ++live_count_;
  }
  ~Node() {
    --live_count_;
  }

  std::string name;

  // Owners point down, observers point up.
  std::weak_ptr<Node> parent;
  std::vector<std::shared_ptr<Node>> children;

  [[nodiscard]] static int live_count() noexcept {
    return live_count_;
  }

private:
  static inline int live_count_ = 0;
};

std::shared_ptr<Node> add_child(const std::shared_ptr<Node>& parent, std::string name) {
  auto child = std::make_shared<Node>(std::move(name));
  child->parent = parent;
  parent->children.push_back(child);
  return child;
}

std::vector<std::string> path_to_root(const std::shared_ptr<Node>& node) {
  std::vector<std::string> names;
  for (auto current = node; current != nullptr; current = current->parent.lock()) {
    names.push_back(current->name);
  }
  return {names.rbegin(), names.rend()};
}

TEST_CASE("a tree is destroyed when its root is") {
  CHECK(Node::live_count() == 0);
  {
    const auto root = std::make_shared<Node>("root");
    const auto branch = add_child(root, "branch");
    add_child(branch, "leaf");
    CHECK(Node::live_count() == 3);
  }
  CHECK(Node::live_count() == 0);
}

TEST_CASE("path_to_root walks up through weak links") {
  const auto root = std::make_shared<Node>("root");
  const auto branch = add_child(root, "branch");
  const auto leaf = add_child(branch, "leaf");

  CHECK(path_to_root(leaf) == std::vector<std::string>{"root", "branch", "leaf"});
  CHECK(path_to_root(root) == std::vector<std::string>{"root"});
}

TEST_CASE("a weak_ptr knows when its object is gone") {
  std::weak_ptr<Node> observer;
  {
    const auto node = std::make_shared<Node>("temporary");
    observer = node;
    CHECK_FALSE(observer.expired());
    CHECK(observer.lock() != nullptr);
  }
  CHECK(observer.expired());
  CHECK(observer.lock() == nullptr);
}

TEST_CASE("a weak parent does not keep the parent alive") {
  std::shared_ptr<Node> leaf;
  {
    const auto root = std::make_shared<Node>("root");
    leaf = add_child(root, "leaf");
    CHECK(Node::live_count() == 2);
  }
  CHECK(Node::live_count() == 1);
  CHECK(path_to_root(leaf) == std::vector<std::string>{"leaf"});
}
