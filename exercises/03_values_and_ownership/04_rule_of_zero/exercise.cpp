// =============================================================================
//  03.04 -- The rule of zero
// =============================================================================
//
//  You have now written copy and move operations by hand twice. The lesson of
//  this exercise is that you should almost never do it again.
//
//  THE RULE OF ZERO (Core Guidelines C.20): if your class can avoid declaring
//  any destructor, copy or move operation, it should. Hold members that manage
//  their own resources -- std::vector, std::string, std::unique_ptr -- and the
//  compiler generates all five operations correctly, for free, forever.
//
//  Every hand-written special member is code that can be wrong, and code that
//  must be updated when you add a member. The compiler's version is updated
//  automatically.
//
//  The trap is that these operations are *linked*. Declaring any one of them
//  changes what the others do:
//
//      declaring a destructor  -> move operations are NOT generated
//                                 (copies still are, but deprecated)
//      declaring a copy op     -> move operations are NOT generated
//      declaring a move op     -> copy operations are DELETED
//
//  "Not generated" means "silently falls back to copying". That is how a class
//  loses its move semantics without anyone editing a move constructor: someone
//  added an empty destructor for a logging statement.
//
//  TASK
//    `Document` below declares three special members it does not need, and has
//    lost its move operations as a result. Delete them all and let the
//    compiler do the work. The tests check that the generated operations are
//    correct *and* that moves are still moves.
//
//
//  NOTE  This exercise starts as a compile error, and the error is the lesson:
//        the static_asserts below fail because the hand-written copy
//        constructor and destructor suppressed the move operations.
//
//  RUN IT
//    ./mcpp test 03_04
//
// =============================================================================

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

class Document {
public:
  Document() = default;
  Document(std::string title, std::vector<std::string> paragraphs)
      : title_(std::move(title)), paragraphs_(std::move(paragraphs)) {}

  // TODO: delete all three of the following. Every member of this class
  // already knows how to copy, move and destroy itself; these hand-written
  // versions add nothing except the loss of move semantics.
  Document(const Document& other)
      : title_(other.title_), paragraphs_(other.paragraphs_) {}

  Document& operator=(const Document& other) {
    if (this != &other) {
      title_ = other.title_;
      paragraphs_ = other.paragraphs_;
    }
    return *this;
  }

  ~Document() {}

  [[nodiscard]] const std::string& title() const noexcept {
    return title_;
  }
  [[nodiscard]] std::size_t paragraph_count() const noexcept {
    return paragraphs_.size();
  }

private:
  std::string title_;
  std::vector<std::string> paragraphs_;
};

TEST_CASE("the compiler's copy operations are correct") {
  const Document original{"Report", {"first", "second"}};
  Document copy = original;
  CHECK(copy.title() == "Report");
  CHECK(copy.paragraph_count() == 2);

  Document assigned;
  assigned = original;
  CHECK(assigned.title() == "Report");
}

TEST_CASE("the move operations exist and are noexcept") {
  // These fail while the hand-written copy constructor and destructor are
  // still there: declaring them suppresses the implicit move operations.
  static_assert(std::is_nothrow_move_constructible_v<Document>);
  static_assert(std::is_nothrow_move_assignable_v<Document>);
  CHECK(true);
}

TEST_CASE("moving really moves") {
  Document source{"Big", {"a", "b", "c"}};
  const Document target = std::move(source);
  CHECK(target.paragraph_count() == 3);
  // Not a guarantee of Document -- a guarantee of std::vector's move
  // constructor, which Document now inherits for free.
  CHECK(source.paragraph_count() == 0);
}

TEST_CASE("rule of zero survives adding a member") {
  // Every special member is regenerated when the class changes. Add a member
  // to Document and nothing below needs editing -- which is the point.
  static_assert(std::is_copy_constructible_v<Document>);
  static_assert(std::is_move_constructible_v<Document>);
  static_assert(std::is_destructible_v<Document>);
  CHECK(true);
}
