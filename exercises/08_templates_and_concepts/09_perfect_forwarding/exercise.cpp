// =============================================================================
//  08.09 -- Forwarding references and std::forward
// =============================================================================
//
//  `T&&` means two different things depending on where it appears.
//
//      void f(Widget&& w);                    an rvalue reference. Binds only
//                                             to rvalues.
//
//      template <typename T> void f(T&& w);   a FORWARDING reference. Binds to
//                                             anything, and remembers which.
//
//  The second works through REFERENCE COLLAPSING. When you pass an lvalue,
//  T is deduced as `Widget&`, so the parameter becomes `Widget& &&`, which
//  collapses to `Widget&`. Pass an rvalue and T is `Widget`, so the parameter
//  is `Widget&&`. One signature, both categories preserved -- in T.
//
//  `auto&&` is a forwarding reference too, and so is the `auto&&` in a
//  range-for.
//
//  `std::forward<T>(x)` is the cast that gets the category back OUT of T. It
//  is a conditional move: an rvalue if T was deduced from an rvalue, an lvalue
//  otherwise. Without it, everything you pass on is an lvalue -- because a
//  named parameter always is (03.02) -- and your "perfect" forwarding copies.
//
//      std::move    unconditional cast to rvalue.    Use on a concrete type.
//      std::forward conditional, driven by T.        Use on a T&& parameter.
//                                                    Always with the <T>.
//
//  The trap: a forwarding-reference constructor is greedier than the copy
//  constructor. `template <typename T> Wrapper(T&&)` beats
//  `Wrapper(const Wrapper&)` when passed a non-const Wrapper lvalue, because
//  the template matches exactly. Constrain it (08.06) or overload carefully.
//
//  TASK
//    Make `Factory::create` and `apply_twice` forward properly, and stop
//    `Holder`'s constructor from hijacking copies.
//
//
//  NOTE  This exercise starts as a compile error, and the error is the third
//        bug: the forwarding constructor is chosen for a Holder argument and
//        then tries to build a Tracked out of it.
//
//  RUN IT
//    ./mcpp test 08_09
//
// =============================================================================

#include <doctest/doctest.h>

#include <concepts>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

struct Tracked {
  Tracked() = default;
  explicit Tracked(std::string name) : name(std::move(name)) {}

  Tracked(const Tracked& other) : name(other.name) {
    ++copies;
  }
  Tracked(Tracked&& other) noexcept : name(std::move(other.name)) {
    ++moves;
  }
  Tracked& operator=(const Tracked&) = default;
  Tracked& operator=(Tracked&&) noexcept = default;
  ~Tracked() = default;

  std::string name;

  static void reset() {
    copies = 0;
    moves = 0;
  }
  static inline int copies = 0;
  static inline int moves = 0;
};

struct Widget {
  Tracked payload;
  int count = 0;

  Widget(Tracked payload, int count) : payload(std::move(payload)), count(count) {}
};

// TODO: forward the arguments to Widget's constructor. As written, `args` are
// named parameters -- lvalues -- so every argument is copied even when the
// caller passed a temporary.
template <typename... Args>
std::unique_ptr<Widget> create(Args&&... args) {
  return std::make_unique<Widget>(args...);
}

// Calls `f` twice: once with a copy, and once with the original.
//
// TODO: the second call is the last use of `value`, so it should forward.
// The first must not -- `value` is still needed.
template <typename F, typename T>
void apply_twice(F&& f, T&& value) {
  f(value);
  f(value);
}

// TODO: this constructor is greedier than the copy constructor: passing a
// non-const `Holder` lvalue selects the template (an exact match) rather than
// the copy constructor (which needs a const conversion). Constrain it so that
// it does not accept a Holder.
//
// `std::same_as<std::remove_cvref_t<T>, Holder>` is the condition to exclude.
class Holder {
public:
  template <typename T>
  explicit Holder(T&& value) : payload_(std::forward<T>(value)) {}

  Holder(const Holder&) = default;
  Holder(Holder&&) noexcept = default;
  Holder& operator=(const Holder&) = default;
  Holder& operator=(Holder&&) noexcept = default;
  ~Holder() = default;

  [[nodiscard]] const Tracked& payload() const noexcept {
    return payload_;
  }

private:
  Tracked payload_;
};

TEST_CASE("forwarding preserves the value category") {
  Tracked::reset();

  // An rvalue argument should be MOVED all the way into the Widget.
  const auto moved = create(Tracked{"temp"}, 1);
  CHECK(moved->payload.name == "temp");
  CHECK(Tracked::copies == 0);

  Tracked::reset();

  // An lvalue argument must still be copied -- the caller keeps it.
  Tracked owned{"kept"};
  const auto copied = create(owned, 2);
  CHECK(copied->payload.name == "kept");
  CHECK(owned.name == "kept");
  CHECK(Tracked::copies == 1);
}

TEST_CASE("forward only on the last use") {
  std::vector<std::string> seen;
  const auto record = [&seen](const Tracked& value) { seen.push_back(value.name); };

  apply_twice(record, Tracked{"twice"});
  CHECK(seen == std::vector<std::string>{"twice", "twice"});
}

TEST_CASE("a forwarding constructor must not hijack copies") {
  const Tracked payload{"held"};
  Holder original{payload};

  Tracked::reset();

  // `original` is a non-const lvalue. Without the constraint, the template
  // constructor wins and tries to build a Tracked from a Holder.
  const Holder copy{original};
  CHECK(copy.payload().name == "held");

  const Holder& const_reference = original;
  const Holder from_const{const_reference};
  CHECK(from_const.payload().name == "held");
}

TEST_CASE("reference collapsing, spelled out") {
  // These are the deductions the rules above produce.
  const auto deduce = []<typename T>(T&&) { return std::is_lvalue_reference_v<T>; };

  int lvalue = 0;
  CHECK(deduce(lvalue));  // T = int&,  parameter int&
  CHECK_FALSE(deduce(0)); // T = int,   parameter int&&
  CHECK_FALSE(deduce(std::move(lvalue)));
}
