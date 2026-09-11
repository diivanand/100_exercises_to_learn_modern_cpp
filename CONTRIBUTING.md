# Contributing

## Adding an exercise

1. Create `exercises/<NN_chapter>/<NN_name>/exercise.cpp` and
   `solutions/<NN_chapter>/<NN_name>/exercise.cpp`. CMake globs the tree, so
   there is nothing to register — reconfigure and the target exists.

2. Write the **solution first**. It is the specification, and writing it first
   is the only reliable way to find out whether the exercise is well posed.

3. Derive the starter from it: keep the tests verbatim, replace the
   implementation with a stub or a plausibly-wrong version, and mark it `TODO`.

4. Run `./scripts/check-course.sh`.

## What the checker enforces

- **Every solution builds and passes.** A solution that does not is worse than
  no solution.
- **Every starter fails** — to compile or to pass. An exercise that already
  passes teaches nothing, and this is the most common way for the course to
  rot as the compiler or library changes underneath it.
- **A starter that does not compile says so** in a `NOTE` line in its header
  comment, so nobody is left staring at an error the course did not warn them
  about. The converse is checked too: a `NOTE` that promises a compile error
  fails the check if the starter in fact builds.

## What makes a good exercise here

- **The header comment is the teaching material.** There is no separate book,
  so the comment has to explain the idea, why it exists, and what goes wrong
  without it. Cite the Core Guidelines rule by number where there is one.

- **The bug should be one somebody would really write.** A starter that fails
  because a function returns `0` teaches nothing. A starter that fails because
  it captured by reference in a lambda that escapes teaches the thing.

- **The tests are the specification.** Include at least one test that catches a
  plausible shortcut — the copy that looks like a move, the read that silently
  inserts, the view that dangles.

- **Prefer a failure the learner can diagnose.** A wrong value with a printed
  comparison beats a segfault; where the bug genuinely is memory corruption,
  say in the header that it should be run under `asan`.

- **Concurrency exercises must be deterministic.** A test that fails one run in
  fifty is worse than no test. Where the bug is a race, either make the wrong
  version fail overwhelmingly (enough iterations that the probability is
  negligible), or test the *intent* — the declared memory ordering, the
  declared `noexcept` — rather than the outcome.

## Style

`.clang-format` and `.clang-tidy` are in the repository root and are not up for
debate. Run `./mcpp format` before committing.

Exercise prose is British-flavoured plain English: short sentences, no
exclamation marks, no "simply", no "obviously".
