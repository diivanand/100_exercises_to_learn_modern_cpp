// Solution -- 03.04 The rule of zero
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

  // No destructor, no copy operations, no move operations. All five are
  // generated, all five are correct, and they stay correct as members change.

  const std::string& title() const noexcept {
    return title_;
  }
  std::size_t paragraph_count() const noexcept {
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
  static_assert(std::is_nothrow_move_constructible_v<Document>);
  static_assert(std::is_nothrow_move_assignable_v<Document>);
  CHECK(true);
}

TEST_CASE("moving really moves") {
  Document source{"Big", {"a", "b", "c"}};
  const Document target = std::move(source);
  CHECK(target.paragraph_count() == 3);
  // `source` is valid but unspecified (see 03.02): asserting it is empty would
  // be testing the library. What Document guarantees is that the moved-from
  // object can still be assigned to and destroyed.
  source = Document{"Small", {"z"}};
  CHECK(source.paragraph_count() == 1);
}

TEST_CASE("rule of zero survives adding a member") {
  static_assert(std::is_copy_constructible_v<Document>);
  static_assert(std::is_move_constructible_v<Document>);
  static_assert(std::is_destructible_v<Document>);
  CHECK(true);
}
