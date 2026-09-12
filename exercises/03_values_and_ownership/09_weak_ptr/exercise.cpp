// =============================================================================
//  03.09 -- std::weak_ptr and the cycle problem
// =============================================================================
//
//  Reference counting has one failure mode: a cycle. If A holds a shared_ptr
//  to B and B holds one back to A, neither count ever reaches zero, and both
//  objects leak -- quietly, with no warning, for the life of the process.
//
//  `std::weak_ptr<T>` is a non-owning observer of a shared_ptr's object. It
//  does not contribute to the count, so it cannot keep anything alive, and it
//  knows when the object is gone:
//
//      if (auto locked = weak.lock()) { use(*locked); }   // may be nullptr
//
//  `lock()` is the only safe way to use one: it either hands you a real
//  shared_ptr, taking a reference for as long as you hold it, or it hands you
//  nothing. Checking `expired()` and then dereferencing is a race in
//  multi-threaded code -- the answer can change between the two calls.
//
//  The usual rule: OWNERS POINT DOWN, OBSERVERS POINT UP. A parent owns its
//  children with shared_ptr; each child refers to its parent with weak_ptr.
//
//  TASK
//    Break the cycle in the tree below, then implement `path_to_root` so it
//    walks up through the weak parent links.
//
//  NOTE  This exercise compiles as it stands, and that is the point: a cycle
//        is not an error the compiler can see. Expect the live-count tests
//        to fail, because nothing is ever freed.
//
//  RUN IT
//    ./mcpp test 03_09
//
// =============================================================================

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

  // TODO: a child holding a shared_ptr to its parent, while the parent holds
  // shared_ptrs to its children, is a cycle: neither ever reaches zero. Make
  // `parent` a std::weak_ptr<Node>.
  std::shared_ptr<Node> parent;
  std::vector<std::shared_ptr<Node>> children;

  static int live_count() noexcept {
    return live_count_;
  }

private:
  static inline int live_count_ = 0;
};

// Links `child` under `parent`, in both directions.
std::shared_ptr<Node> add_child(const std::shared_ptr<Node>& parent, std::string name) {
  auto child = std::make_shared<Node>(std::move(name));
  child->parent = parent;
  parent->children.push_back(child);
  return child;
}

// Returns the names from `node` up to the root, root first.
//
// TODO: walk up the parent chain. Each step needs `lock()`, because a weak_ptr
// cannot be dereferenced -- and the loop should stop when lock() returns null,
// which is how you know you have reached the root (or that the tree above you
// has been destroyed).
std::vector<std::string> path_to_root(const std::shared_ptr<Node>& node) {
  std::vector<std::string> names;
  for (auto current = node; current != nullptr; current = current->parent) {
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
  // With a shared_ptr parent link this stays at 3 forever: a textbook leak.
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
  // The root is gone; the leaf survives because we still hold it, and knows
  // that its parent has disappeared.
  CHECK(Node::live_count() == 1);
  CHECK(path_to_root(leaf) == std::vector<std::string>{"leaf"});
}
