// =============================================================================
//  06.03 -- push_back, emplace_back, and where the copies come from
// =============================================================================
//
//  `push_back(x)` takes an object and puts a copy (or a move) of it into the
//  container. `emplace_back(args...)` forwards the arguments to T's
//  constructor and builds the object IN PLACE -- no temporary at all.
//
//      v.push_back(Widget{name, size});   // construct, then move
//      v.emplace_back(name, size);        // construct, once, in the vector
//
//  When it does not matter: `v.push_back(std::move(w))` and
//  `v.emplace_back(std::move(w))` do the same thing. For an int, likewise.
//
//  When it does: building the object costs an allocation, or the type is
//  expensive to move, or it cannot be moved at all.
//
//  What emplace costs you:
//
//   * IT BYPASSES explicit. `emplace_back` uses direct-initialisation, so a
//     constructor you marked `explicit` (04.02) is reachable without saying
//     the type's name. That is occasionally what you want and usually not.
//   * IT IS LESS READABLE. `emplace_back(1, 2, 3)` does not say what is built.
//   * A one-argument `emplace_back` can silently pick a converting
//     constructor.
//
//  So: reach for push_back by default, emplace_back when you are building the
//  element from its parts.
//
//  Both return differently, too: `emplace_back` returns a reference to the new
//  element (since C++17); `push_back` returns void.
//
//  TASK
//    Remove the unnecessary temporaries below, and use the return value of
//    emplace_back.
//
//  RUN IT
//    ./mcpp test 06_03
//
// =============================================================================

#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

class Session {
public:
  Session(std::string user, int id) : user_(std::move(user)), id_(id) {
    ++constructions_;
  }

  Session(const Session& other) : user_(other.user_), id_(other.id_) {
    ++copies_;
  }
  Session(Session&& other) noexcept : user_(std::move(other.user_)), id_(other.id_) {
    ++moves_;
  }
  Session& operator=(const Session&) = default;
  Session& operator=(Session&&) noexcept = default;
  ~Session() = default;

  [[nodiscard]] const std::string& user() const noexcept {
    return user_;
  }
  [[nodiscard]] int id() const noexcept {
    return id_;
  }

  [[nodiscard]] static int constructions() noexcept {
    return constructions_;
  }
  [[nodiscard]] static int copies() noexcept {
    return copies_;
  }
  [[nodiscard]] static int moves() noexcept {
    return moves_;
  }

  static void reset_counts() {
    constructions_ = 0;
    copies_ = 0;
    moves_ = 0;
  }

private:
  static inline int constructions_ = 0;
  static inline int copies_ = 0;
  static inline int moves_ = 0;

  std::string user_;
  int id_;
};

// TODO: build each Session directly in the vector. Right now every element is
// constructed, then moved, then destroyed.
std::vector<Session> open_sessions(const std::vector<std::string>& users) {
  std::vector<Session> sessions;
  sessions.reserve(users.size());
  int id = 1;
  for (const auto& user : users) {
    sessions.push_back(Session{user, id++});
  }
  return sessions;
}

// Adds one session and returns a reference to it.
//
// TODO: `emplace_back` already returns a reference to the new element. Use it
// rather than looking the element up again.
Session& open_one(std::vector<Session>& sessions, std::string user, int id) {
  sessions.emplace_back(std::move(user), id);
  return sessions[sessions.size() - 1];
}

TEST_CASE("emplace_back constructs in place") {
  Session::reset_counts();

  const std::vector<Session> sessions = open_sessions({"ada", "alan", "grace"});

  CHECK(sessions.size() == 3);
  CHECK(sessions[0].user() == "ada");
  CHECK(sessions[2].id() == 3);

  CHECK(Session::constructions() == 3);
  // Constructed straight into the vector's storage: nothing to move.
  CHECK(Session::moves() == 0);
  CHECK(Session::copies() == 0);
}

TEST_CASE("push_back of an lvalue still copies -- as it must") {
  Session::reset_counts();

  std::vector<Session> sessions;
  sessions.reserve(2);

  const Session existing{"ada", 1};
  sessions.push_back(existing); // the caller still owns `existing`
  CHECK(Session::copies() == 1);

  Session temporary{"alan", 2};
  sessions.push_back(std::move(temporary)); // the caller has given it up
  CHECK(Session::moves() == 1);
}

TEST_CASE("emplace_back returns the new element") {
  std::vector<Session> sessions;
  sessions.reserve(2);

  Session& first = open_one(sessions, "ada", 1);
  CHECK(first.user() == "ada");
  CHECK(&first == &sessions.front());

  Session& second = open_one(sessions, "alan", 2);
  CHECK(second.id() == 2);
  CHECK(&second == &sessions.back());
}
