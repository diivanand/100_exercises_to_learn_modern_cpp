// Solution -- 06.03 push_back, emplace_back
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

std::vector<Session> open_sessions(const std::vector<std::string>& users) {
  std::vector<Session> sessions;
  sessions.reserve(users.size());
  int id = 1;
  for (const auto& user : users) {
    // The arguments are forwarded to Session's constructor; no Session
    // temporary is ever created.
    sessions.emplace_back(user, id++);
  }
  return sessions;
}

Session& open_one(std::vector<Session>& sessions, std::string user, int id) {
  return sessions.emplace_back(std::move(user), id);
}

TEST_CASE("emplace_back constructs in place") {
  Session::reset_counts();

  const std::vector<Session> sessions = open_sessions({"ada", "alan", "grace"});

  CHECK(sessions.size() == 3);
  CHECK(sessions[0].user() == "ada");
  CHECK(sessions[2].id() == 3);

  CHECK(Session::constructions() == 3);
  CHECK(Session::moves() == 0);
  CHECK(Session::copies() == 0);
}

TEST_CASE("push_back of an lvalue still copies -- as it must") {
  Session::reset_counts();

  std::vector<Session> sessions;
  sessions.reserve(2);

  const Session existing{"ada", 1};
  sessions.push_back(existing);
  CHECK(Session::copies() == 1);

  Session temporary{"alan", 2};
  sessions.push_back(std::move(temporary));
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
