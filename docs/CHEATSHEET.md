# Modern C++ cheatsheet

A one-page reminder of the decisions this course keeps asking you to make.
Each entry names the exercise that covers it.

## Choosing a parameter type — 02.06

| The function... | Takes |
|---|---|
| reads something cheap to copy (`int`, `string_view`, `span`) | by value `T` |
| reads something expensive to copy | `const T&` |
| keeps a copy (a "sink") | by value `T`, then `std::move` |
| modifies the caller's object | `T&` |
| forwards to something else | `T&&` + `std::forward<T>` (08.09) |

Prefer returning a value to filling in an out-parameter.

## Choosing an ownership type — 03.07, 03.08, 03.09

| Situation | Use |
|---|---|
| the object has one owner | `std::unique_ptr<T>` |
| ownership is genuinely shared, and its end unpredictable | `std::shared_ptr<T>` |
| observing something you do not own, that may vanish | `std::weak_ptr<T>` |
| observing something guaranteed to outlive you | `T&` or `T*` |
| the object is a value | just a `T` |

`std::make_unique` / `std::make_shared`, not bare `new`.

## The special members — 03.04, 03.05

**Rule of zero**: declare none of them. Hold members that manage themselves.
This is right for almost every class.

**Rule of five**: if you declare any of the five, consider all five —
often as `= default` or `= delete`.

Declaring any of these *suppresses* others:

| You declare | You lose |
|---|---|
| a destructor | the implicit move operations |
| a copy operation | the implicit move operations |
| a move operation | the copy operations (deleted) |

Move operations should be `noexcept` (02.04), or containers will copy instead.

## Error handling — 05.01, 05.04, 05.07, 05.08

| The failure is... | Use |
|---|---|
| a bug in the caller | `assert` |
| impossible by construction | `static_assert` |
| bad external input, handled far away | an exception |
| bad external input, handled right here | `std::optional` / a result type |
| "there is no value", with no reason needed | `std::optional` |
| "it failed, and here is why" | `std::expected` (C++23) or a variant |

Guarantee levels: **nothrow** > **strong** (transactional) > **basic**
(valid, no leaks) > none. Basic is the minimum acceptable.

## Containers — chapter 06

| Need | Use |
|---|---|
| a sequence | `std::vector` — the default |
| a fixed-size sequence, no heap | `std::array` |
| lookup by key | `std::unordered_map` |
| lookup by key, iterated in order | `std::map` |
| a read-only view of characters | `std::string_view` |
| a view of contiguous elements | `std::span` |

Traps: `operator[]` on a map *inserts* (06.06); a `push_back` may invalidate
every iterator, pointer and reference (06.02); `std::remove` does not remove
(06.09) — use C++20's `std::erase`/`std::erase_if`.

## const and constexpr — 01.04, 01.05

| Keyword | Means |
|---|---|
| `const` | I will not modify this through this name |
| `constexpr` (variable) | known at compile time, and `const` |
| `constexpr` (function) | *may* run at compile time |
| `consteval` | *must* run at compile time |
| `constinit` | initialised at compile time, but mutable |

## Concurrency — chapter 10

| Need | Use |
|---|---|
| a thread | `std::jthread` (10.01) |
| one shared value | `std::atomic<T>` (10.06) |
| several values changing together | `std::mutex` + `std::lock_guard` (10.03) |
| two mutexes at once | `std::scoped_lock` (10.03) |
| many readers, few writers | `std::shared_mutex` (10.03) |
| wait for a condition | `std::condition_variable` + predicate (10.04) |
| a value computed elsewhere | `std::async(std::launch::async, …)` (10.05) |
| wait for N events, once | `std::latch` (10.08) |
| a repeated rendezvous | `std::barrier` (10.08) |
| limit concurrency to K | `std::counting_semaphore` (10.08) |
| cancellation | `std::stop_token` (10.09) |

Memory ordering: use the default (`seq_cst`) unless you have measured.
`release`/`acquire` come as a pair or not at all (10.07).

## Ranges — chapter 07

```cpp
values | std::views::filter(pred)      // lazy
       | std::views::transform(f)      // lazy
       | std::views::take(n)           // lazy
```

Nothing runs until something iterates; a view is re-evaluated every time it is
traversed (07.04); a view must not outlive what it refers to (07.08).

Prefer the `std::ranges::` algorithms, and use projections instead of
member-reaching lambdas: `std::ranges::sort(people, {}, &Person::age)` (07.02).

## The tools

```sh
./mcpp format --check              # formatting
./mcpp tidy                        # clang-tidy, incl. cppcoreguidelines-*
cmake --preset asan && ctest --preset asan   # memory + UB
cmake --preset tsan && ctest --preset tsan   # data races
```

The compiler is the cheapest analyser you have. `-Wall -Wextra -Wconversion
-Werror` is on by default in this project, and should be on in yours.

## CUDA — chapters 14 to 17

| Need | Use |
|---|---|
| an error you cannot ignore | `check(cudaMalloc(...))` — throws with file:line (14.02) |
| the error from a launch | `check(cudaGetLastError())` right after `<<<>>>` (14.02) |
| device memory that frees itself | `DeviceBuffer<T>` (14.03) or `thrust::device_vector` (16.01) |
| a grid that covers `n` | `(n + block - 1) / block` blocks, plus a bounds guard (14.04) |
| a kernel for any `n` | a grid-stride loop (14.01) |
| a copy that can overlap | pinned host memory + `cudaMemcpyAsync` on a stream (15.06) |
| a dependency between streams | `cudaEventRecord` then `cudaStreamWaitEvent` (14.06) |
| fast global memory | consecutive threads read consecutive addresses (15.01) |
| a tile reused by a block | `__shared__`, `__syncthreads`, pad to dodge bank conflicts (15.02) |
| a warp-wide sum | `__shfl_down_sync` or `cg::reduce` on a 32-wide tile (15.03, 16.04) |
| a block size | `cudaOccupancyMaxPotentialBlockSize` (15.04) |
| many small launches | capture them into a CUDA graph (16.05) |
| allocation in a hot loop | `cudaMallocAsync` from a pool with the release threshold raised (16.06) |
| "where did the time go?" | `./mcpp profile 17_02` (Nsight Systems) |
| "why is this kernel slow?" | `./mcpp profile 17_03 --ncu` (Nsight Compute) |
| memory errors and races | `compute-sanitizer --tool memcheck|racecheck|initcheck` (17.04) |

Time GPU work with events, after a warm-up, and take the best of several
runs (17.01). Never `std::chrono` around an asynchronous launch.

A `1.0` literal or `pow` in device code is double precision, and a GeForce
card runs double at 1/64 rate: write `1.0F` and `x * x` (17.03).
