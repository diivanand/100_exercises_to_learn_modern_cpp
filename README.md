# 100 exercises to learn modern C++

Learn C++20 by fixing, finishing and writing 100 small programs.

Every exercise is a single self-contained file that states a problem and checks
your answer. You edit the code, run the tests, and move on when they pass.
There is no lecture to sit through and nothing to read that is not next to the
code it is about.

This is the C++ counterpart to
[100 exercises to learn Rust](https://github.com/mainmatter/100-exercises-to-learn-rust).

## Requirements

- **A recent C++20 toolchain.** The course uses `std::format`, `std::jthread`
  and the full `<ranges>`, which arrived in the standard libraries later than
  the core language did. What CI builds, and what is known to work:
  - GCC 13 or newer (libstdc++ 13 is the first with `<format>`);
  - Clang 18 or newer, with either libstdc++ 13+ or libc++ 18+;
  - macOS: Xcode 16 or newer. `xcode-select --install` is enough.
  Older toolchains will build most chapters, but not 10, 12 or 13.
- **CMake 3.24+** and **Ninja** — `brew install cmake ninja`.
- Optionally **clang-format** and **clang-tidy** — `brew install llvm`.

Nothing else. doctest is vendored in `third_party/`, so the project configures
and builds with no network access.

## Getting started

```sh
git clone <this repo>
cd 100-exercises-to-learn-modern-cpp
./mcpp next
```

`./mcpp next` builds and runs the first exercise that is not yet passing, and
shows you its failure. That is the whole loop:

1. Run `./mcpp next`.
2. Open the file it names.
3. Make the tests pass.
4. Repeat.

When every exercise passes, you are done.

## The commands

```
./mcpp next                 the main loop: run the first unfinished exercise
./mcpp test <filter>        run one exercise    (./mcpp test 03_07, or unique_ptr)
./mcpp verify               run everything, as CI does
./mcpp list                 the curriculum, and where you are in it
./mcpp solution <filter>    diff your work against the reference solution
./mcpp build <filter>       build without running
./mcpp format [--check]     clang-format the tree
./mcpp tidy [filter]        clang-tidy the tree
./mcpp clean                remove the build directories
```

A filter is any substring of an exercise id, so `07`, `07_03`, `filter_transform`
and `03_filter_transform` all select 07.03.

## How an exercise works

Each `exercises/<chapter>/<exercise>/exercise.cpp` has three parts:

- **A header comment** explaining the idea, why it exists, and where it bites.
  This is the teaching material; there is no separate book.
- **The code you edit**, marked with `TODO`.
- **The tests**, which are the specification. Read them — several exercises
  have a test that exists specifically to catch a plausible shortcut.

Some exercises **start as a compile error**, because for some features the
compiler *is* the test — you cannot run a `static_assert` that does not build.
Those say so in a `NOTE` in their header comment.

## Some exercises are meant to be run under a sanitizer

A use-after-free that happens to produce the right answer is still a
use-after-free. Where an exercise is about a lifetime bug, its header says to
re-run it under a sanitizer:

```sh
cmake --preset asan && ctest --preset asan       # memory + undefined behaviour
cmake --preset tsan && ctest --preset tsan       # data races (chapter 10)
```

Learning to reach for these is part of the course.

## The curriculum

| # | Chapter | What it covers |
|---|---------|----------------|
| 00 | Getting started | the workflow, reading a failure, taking warnings seriously |
| 01 | Types and initialization | `auto`, braces and narrowing, `nullptr`, `const`/`constexpr`/`consteval`/`constinit`, structured bindings, init-statements, scoped enums |
| 02 | Functions and control flow | range-`for`, `if constexpr`, `[[nodiscard]]`, `noexcept`, designated initializers, choosing a parameter type |
| 03 | Values and ownership | value semantics, rvalue references, move operations, rule of zero and of five, RAII, `unique_ptr`/`shared_ptr`/`weak_ptr` |
| 04 | Classes and polymorphism | const correctness, `explicit`, delegating and inheriting constructors, `override`/`final`, virtual destructors, `= default`/`= delete`, operator overloading, `<=>`, type erasure |
| 05 | Errors and alternatives | exceptions, the safety guarantees, scope guards, `optional`, `variant`, `visit`, result types, invariants and asserts |
| 06 | Containers and strings | `array`, vector growth and invalidation, `emplace` vs `push`, `string_view`, `span`, `map` vs `unordered_map`, `try_emplace`/`extract`, `tuple`, erasing |
| 07 | Algorithms and ranges | algorithms over loops, projections, `filter`/`transform`, laziness, generators, materialising, composing pipelines, dangling views |
| 08 | Templates and concepts | function and class templates, CTAD, parameter packs, fold expressions, NTTPs, SFINAE → concepts, `requires`, designing concepts, perfect forwarding |
| 09 | Lambdas | closures, captures, generic and templated lambdas, `constexpr` lambdas, what `std::function` costs, `bind_front`/`invoke`, immediately-invoked lambdas |
| 10 | Concurrency | `jthread`, data races, mutexes and deadlock, condition variables, `async`/`future`, atomics, memory ordering, latch/barrier/semaphore, `stop_token` |
| 11 | Coroutines | writing a generator, a lazy task, awaiters and symmetric transfer, coroutine lifetimes, composing generators |
| 12 | Modern standard library | `format`, `chrono`, `filesystem`, `regex`, `<bit>`, `source_location` |
| 13 | Capstone | build a metrics store: value types, parsing, invariants, range queries, concurrent ingestion |

## Solutions

Every exercise has a reference solution in `solutions/`, with comments
explaining the choices rather than just the mechanics.

```sh
./mcpp solution 03_07    # diff your work against it
```

Try to reach for them only after you have a failing attempt of your own — the
diff teaches more when you have already made the decision it disagrees with.

To build and test them all:

```sh
cmake --preset solutions && ctest --preset solutions
```

## The build

This is a normal modern CMake project, and it is worth reading:

- `CMakeLists.txt` — interface targets carrying options and warnings, no
  global state.
- `cmake/CompilerWarnings.cmake` — the warning set, from
  [cppbestpractices](https://github.com/cpp-best-practices/cppbestpractices).
  It is aggressive, and `-Werror` is on by default. That is deliberate: the
  compiler is the cheapest static analyser you have.
- `cmake/Sanitizers.cmake` — ASan/UBSan and TSan wiring.
- `cmake/AddExercise.cmake` — exercise discovery, so adding a directory adds a
  target and a test.
- `CMakePresets.json` — the presets the commands above use.

If a warning is genuinely in your way, configure with
`-DMCPP_WARNINGS_AS_ERRORS=OFF`. Use it to keep moving, not as a habit.

### A note on clang-tidy

`.clang-tidy` turns on `bugprone-*`, `cert-*`, `cppcoreguidelines-*`,
`modernize-*`, `performance-*` and `readability-*`, and then switches off about
a dozen checks — each with the reason written next to it in the file.

Those disables exist because this is *teaching* code: several exercises
deliberately demonstrate the thing a check exists to prevent. 03.02 inspects a
moved-from object on purpose, 03.01 walks a raw pointer on purpose, 08.09 has a
greedy forwarding constructor on purpose. Leaving those checks on would bury
the findings about **your** code under complaints about the material.

Read that list before copying the file into a project of your own.

You will also find a handful of `NOLINT` comments in the solutions, each with
an explanation. They are worth reading: one of them documents a case where
clang-tidy's suggestion is *wrong*, and wrong for exactly the reason the
exercise it sits in exists.

The reference solutions are kept clean under this configuration, so if
`./mcpp tidy` reports something, it is about your code.

## Using this with CLion

Open the directory. CLion reads `CMakePresets.json` and offers the presets
(`debug`, `release`, `solutions`, `asan`, `tsan`) directly — pick `debug`.

- **Run one exercise**: pick its `ex_<chapter>_<name>` target.
- **Run everything**: the `exercises` target, then the CTest configuration.
- `.gitignore` already covers `.idea/` and `cmake-build-*/`.
- `.clang-format` and `.clang-tidy` are picked up automatically; turn on
  *Settings → Editor → Code Style → Enable ClangFormat*.

## Where this material comes from

- **[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)** —
  cited by rule number throughout (F.16, C.20, R.20, ES.11, …) so you can read
  the source of any claim.
- **[C++ Best Practices](https://github.com/cpp-best-practices/cppbestpractices)** —
  the tooling and warning setup.
- **[Modern C++ Tutorial: C++11/14/17/20 On the Fly](https://github.com/changkun/modern-cpp-tutorial)** —
  the shape of the language-feature chapters.
- **[100 exercises to learn Rust](https://github.com/mainmatter/100-exercises-to-learn-rust)** —
  the format.

## License

The exercises and solutions are released under CC BY-NC 4.0, matching the Rust
course this is modelled on. doctest is MIT; see `third_party/doctest/`.
