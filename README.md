# 100 exercises to learn modern C++

Learn C++20 by fixing, finishing and writing 100 small programs. Then, if you
have an NVIDIA GPU within reach, carry on into 25 more that teach modern CUDA
C++ and how to profile it (chapters 14 to 17, [below](#the-cuda-track-chapters-14-to-17)).

Every exercise is a single self-contained file that states a problem and checks
your answer. You edit the code, run the tests, and move on when they pass.
There is no lecture to sit through and nothing to read that is not next to the
code it is about.

This is the C++ counterpart to
[100 exercises to learn Rust](https://github.com/mainmatter/100-exercises-to-learn-rust).

## Requirements

- **A recent C++20 toolchain.** The course uses `std::format`, `std::jthread`
  and the full `<ranges>`, which arrived in the standard libraries later than
  the core language did. What is known to work (CI builds the macOS row):
  - GCC 13 or newer (libstdc++ 13 is the first with `<format>`);
  - Clang 18 or newer, with either libstdc++ 13+ or libc++ 18+;
  - macOS: Xcode 16 or newer. `xcode-select --install` is enough.
  Older toolchains will build most chapters, but not 10, 12 or 13.
- **CMake 3.24+** and **Ninja** — `brew install cmake ninja`.
- Optionally **clang-format** and **clang-tidy** — `brew install llvm`.

Nothing else. doctest is vendored in `third_party/`, so the project configures
and builds with no network access. The CUDA chapters additionally need a Linux
machine with an NVIDIA GPU and the CUDA toolkit; without one they are skipped
automatically and the first 100 exercises are unaffected.

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
./mcpp profile <filter>     run a CUDA exercise under Nsight Systems (--ncu: Compute)
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
| 14 | CUDA fundamentals | kernels and launch configuration, error handling, RAII for device memory, 2-D grids and bounds, host/device functions, streams and events |
| 15 | Memory and performance | coalescing, shared-memory tiling and bank conflicts, warp shuffles, occupancy and launch bounds, unified memory and prefetching, pinned memory and overlap, atomics and privatisation |
| 16 | Modern CUDA | Thrust, CUB, libcu++ (`cuda::std::span`, `cuda::atomic_ref`), cooperative groups, CUDA graphs, memory pools, device lambdas and constrained kernel templates |
| 17 | Profiling and capstone | event timing and bandwidth, Nsight Systems timelines with NVTX, Nsight Compute and the FP64 trap, compute-sanitizer, a fused softmax |

Chapters 14 to 17 are `exercise.cu` files and need a GPU; see
[the CUDA track](#the-cuda-track-chapters-14-to-17).

## The CUDA track (chapters 14 to 17)

The last four chapters teach CUDA C++ the way the first thirteen teach C++:
one self-contained file per exercise, the tests are the specification, the
header comment is the book. They assume everything before them, and use it —
`std::span`, `std::source_location`, the rule of five, concepts, lambdas — so
do them in order.

They need hardware the laptop does not have. The intended setup, and the one
the course was written against, is:

- **Write the code on a Mac** (or anything else), in this repository.
- **Build and run it on a Linux machine with an NVIDIA GPU.** The course
  targets an RTX 4090 (Ada Lovelace, `sm_89`); anything Turing or newer works
  with `-DCMAKE_CUDA_ARCHITECTURES=<your number>`.

### Requirements on the GPU machine

- **CUDA Toolkit 12.4 or newer; 13.x recommended.** The exercises use CCCL
  (Thrust, CUB, libcu++) as shipped with the toolkit and avoid everything that
  the CCCL 3.0 clean-up removed, so they build on both 12.x and 13.x.
- **GCC 13 or newer** as the host compiler (CUDA 13.x supports up to GCC 16;
  check the toolkit's release notes for the pairing). The `cuda` preset uses
  `g++` because that is what nvcc is tested against.
- **A driver matching the toolkit** (R580 or newer for CUDA 13).
- **Nsight Systems, Nsight Compute and compute-sanitizer**, which ship with the
  toolkit, for chapter 17. To let a non-root user read the GPU's performance
  counters (Nsight Compute fails with `ERR_NVGPUCTRPERM` otherwise), create
  `/etc/modprobe.d/nvidia-profiling.conf` containing
  `options nvidia NVreg_RestrictProfilingToAdminUsers=0` and reboot.

`nvcc` must be on the PATH (`export PATH=/usr/local/cuda/bin:$PATH`).

### How the build finds CUDA

`cmake/Cuda.cmake` looks for nvcc. If it is found, the `.cu` exercises are
built with `--extended-lambda`, `--expt-relaxed-constexpr` and `-lineinfo`
(so Nsight Compute can attribute stalls to your source lines), and linked
against `CUDA::cudart` and NVTX. If it is not found, they are skipped with a
message and `./mcpp next` prints `skip 14_01_hello_kernel (CUDA)` for each of
them. `-DMCPP_ENABLE_CUDA=ON` turns a missing nvcc into an error, which is
what you want on the GPU machine.

The `cuda`, `cuda-release` and `cuda-solutions` presets exist only on Linux
(CMake hides them elsewhere). `./mcpp` picks `cuda` automatically when it
finds nvcc on a Linux machine, so the workflow is the same on both sides:

```sh
./mcpp next          # skips CUDA exercises on the laptop, runs them on the GPU box
./mcpp test 15_02    # on the laptop: explains that this one needs the GPU machine
./mcpp profile 17_02 # nsys, on the GPU machine
```

CUDA targets are built with `-Wall -Wextra` on the host side and every nvcc
warning on the device side, as warnings rather than errors: the CUDA headers
are not clean under the aggressive set the C++ chapters use, and `-Werror`
would fail on code you do not own.

### Working from CLion on the Mac, building on the GPU machine

CLion's remote development feature does exactly the split above. Once:

1. *Settings → Build, Execution, Deployment → Toolchains*: add a **Remote
   Host** toolchain with the SSH credentials of the GPU machine. Set the
   C++ compiler to `g++`. CLion checks CMake and the compiler over SSH.
2. *Settings → Build, Execution, Deployment → CMake*: add a profile that uses
   that toolchain and the `cuda` preset (CLion lists the presets it reads from
   `CMakePresets.json`; on a remote Linux toolchain the `cuda` ones appear).
3. *Settings → Build, Execution, Deployment → Deployment*: CLion creates an
   automatic upload mapping to a temporary directory on the remote. Exclude
   `cmake-build-*` from the mapping if it did not already.

From then on, editing on the Mac uploads the file, and *Run* builds and runs
on the GPU machine with the output in CLion's console. Pick the
`ex_14_01_hello_kernel` target to run one exercise, `exercises` to build them
all, and the CTest configuration to run everything. `./mcpp` itself is a
shell script; run it in CLion's terminal over SSH, or in any SSH session in
the deployed directory.

For the profilers, the simplest path is an SSH session on the GPU machine:
`./mcpp profile 17_02` writes `cmake-build-cuda/profile_17_02_....nsys-rep`,
which you can open in `nsys-ui` there, or copy back and open in the Nsight
Systems desktop app on the Mac (NVIDIA ships a macOS host for viewing).
The same holds for `--ncu` and `ncu-ui`. Chapter 17's header comments say
what to look for in each.

### Verifying the CUDA solutions

On the GPU machine:

```sh
cmake --preset cuda-solutions && ctest --preset cuda-solutions -L cuda
./scripts/check-course.sh    # includes the CUDA chapters when nvcc is found
```

CI cannot run them (GitHub's hosted runners have no GPU) but it does
**compile** every CUDA solution with nvcc on every push, which catches the
large majority of mistakes.

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
- `cmake/Cuda.cmake` — CUDA detection and the `mcpp::cuda` target (chapters
  14 to 17); skipped, not failed, when there is no nvcc.
- `cmake/AddExercise.cmake` — exercise discovery, so adding a directory adds a
  target and a test. It knows that `exercise.cu` needs `mcpp::cuda`.
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
For the CUDA chapters, see [the CUDA track](#the-cuda-track-chapters-14-to-17)
for the remote-toolchain setup.

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

The CUDA chapters draw on, and cite by section:

- **Kirk & Hwu, *Programming Massively Parallel Processors* (1st ed., 2010)** —
  the thread, memory and performance model. Its hardware numbers are
  Fermi-era, and each exercise says what has changed on Ada.
- **Paulo Motta, *GPU Programming with C++ and CUDA* (Packt, 2025)** — event
  timing, streams, overlap, Nsight Compute, Thrust.
- **Cautaerts & Ghorbanfekr, *GPU-Accelerated Computing with Python 3 and
  CUDA* (Packt, 2026)** — the profiling chapters (Nsight Systems and Compute,
  memory- versus compute-bound kernels), occupancy, warp shuffles.
- The **CUDA C++ Programming Guide** and **Best Practices Guide**, and the
  **CCCL** documentation, checked against CUDA 13.x (September 2026).

## License

The exercises and solutions are released under CC BY-NC 4.0, matching the Rust
course this is modelled on. doctest is MIT; see `third_party/doctest/`.
