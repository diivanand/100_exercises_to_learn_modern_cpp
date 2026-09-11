#!/usr/bin/env bash
#
# Course integrity check. This is what CI runs, and what you should run after
# editing an exercise. It asserts three things:
#
#   1. Every reference solution compiles and passes. A solution that does not
#      is worse than no solution.
#   2. Every exercise STARTER fails -- either to compile or to pass. An
#      exercise that already passes teaches nothing, and is the most common way
#      for one of these to rot.
#   3. Every starter that fails to *compile* says so in its header comment, so
#      a learner is never staring at an error the course did not warn them
#      about -- and, conversely, no header promises a compile error that the
#      starter does not produce.
#
# Usage: scripts/check-course.sh

set -uo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/cmake-build-check"

readonly RED=$'\033[31m' GREEN=$'\033[32m' YELLOW=$'\033[33m' BOLD=$'\033[1m'
readonly RESET=$'\033[0m'

# macOS has no coreutils `timeout`. Some exercise starters loop for ever on
# purpose (a thread with no cancellation check, a wait that is never notified),
# so every starter is run with a deadline. A timeout counts as a failure, which
# is exactly what it is.
run_with_deadline() {
  local seconds="$1"
  shift
  perl -e 'alarm shift; exec @ARGV or exit 127' "${seconds}" "$@"
}

failures=0
note() { printf '  %sFAIL%s %s\n' "${RED}" "${RESET}" "$*"; failures=$((failures + 1)); }
ok() { printf '  %s ok %s %s\n' "${GREEN}" "${RESET}" "$*"; }

printf '%s==> configuring%s\n' "${BOLD}" "${RESET}"
# Always reconfigure. CMake is idempotent here, and reusing a directory that
# was configured with different options is how this script starts reporting
# failures that are really its own.
cmake -S "${ROOT}" -B "${BUILD}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ \
  -DMCPP_BUILD_SOLUTIONS=ON -DMCPP_BUILD_EXERCISES=ON >/dev/null || exit 1

printf '\n%s==> solutions must build and pass%s\n' "${BOLD}" "${RESET}"
if ! cmake --build "${BUILD}" --target solutions -- -k 0 >/dev/null 2>&1; then
  printf '%s\n' "$(cmake --build "${BUILD}" --target solutions -- -k 0 2>&1 |
    grep -E '^FAILED' | sed 's/.*dir\//    /')"
  note "one or more solutions do not compile"
fi
if ctest --test-dir "${BUILD}" -R '^solution/' --output-on-failure >/tmp/mcpp-sol.log 2>&1; then
  ok "all solutions pass"
else
  grep -E '^\s+[0-9]+ - ' /tmp/mcpp-sol.log | sed 's/^/    /'
  note "some solutions fail their own tests"
fi

printf '\n%s==> exercise starters must NOT pass%s\n' "${BOLD}" "${RESET}"
# Remove the previously linked binaries first: a starter that used to compile
# and no longer does would otherwise be judged by a stale executable.
rm -rf "${BUILD}/bin/exercises"
cmake --build "${BUILD}" --target exercises -- -k 0 >/tmp/mcpp-ex-build.log 2>&1

uncompilable=()
while IFS= read -r line; do
  # FAILED: exercises/CMakeFiles/ex_03_07_unique_ptr.dir/...
  if [[ "${line}" =~ ex_([0-9]{2}_[0-9]{2}_[a-z0-9_]+)\.dir ]]; then
    uncompilable+=("${BASH_REMATCH[1]}")
  fi
done < <(grep -E '^FAILED' /tmp/mcpp-ex-build.log)

passing=()
while IFS= read -r dir; do
  chapter="$(basename "$(dirname "${dir}")")"
  name="$(basename "${dir}")"
  id="${chapter%%_*}_${name}"
  binary="${BUILD}/bin/exercises/${chapter}/ex_${id}"
  [[ -x "${binary}" ]] || continue
  # Many starters crash rather than merely failing an assertion (reading past
  # the end of a vector, dereferencing null). A crash is a failure; run the
  # binary in a subshell so the shell does not narrate the signal.
  if (run_with_deadline 20 "${binary}" >/dev/null 2>&1); then
    passing+=("${id}")
  fi
done < <(find "${ROOT}/exercises" -mindepth 2 -maxdepth 2 -type d | sort)

if [[ ${#passing[@]} -eq 0 ]]; then
  ok "every starter fails, as it should"
else
  for id in "${passing[@]}"; do
    note "starter ${id} already passes -- there is nothing to do in it"
  done
fi

printf '\n%s==> starters that do not compile must say so%s\n' "${BOLD}" "${RESET}"
if [[ ${#uncompilable[@]} -eq 0 ]]; then
  ok "no starter needs a note"
else
  for id in "${uncompilable[@]}"; do
    chapter_number="${id%%_*}"
    rest="${id#*_}"
    dir="$(find "${ROOT}/exercises/${chapter_number}"_* -maxdepth 1 -type d \
      -name "${rest}" 2>/dev/null | head -1)"
    if [[ -n "${dir}" ]] && grep -q '^//  NOTE' "${dir}/exercise.cpp"; then
      ok "${id} (documented)"
    else
      note "${id} does not compile and has no NOTE in its header comment"
    fi
  done
fi

printf '\n%s==> starters that say they do not compile must not compile%s\n' "${BOLD}" "${RESET}"
# The converse of the previous check. A header that promises a compile error
# when the starter in fact builds and fails at run time sends the learner
# looking for a diagnostic that is not there.
lying=0
while IFS= read -r dir; do
  chapter="$(basename "$(dirname "${dir}")")"
  name="$(basename "${dir}")"
  id="${chapter%%_*}_${name}"
  grep -qiE '^//  NOTE.*(compile error|does not compile|not compile until)' \
    "${dir}/exercise.cpp" || continue
  if [[ " ${uncompilable[*]-} " != *" ${id} "* ]]; then
    note "${id} says it starts as a compile error, but it compiles"
    lying=$((lying + 1))
  fi
done < <(find "${ROOT}/exercises" -mindepth 2 -maxdepth 2 -type d | sort)
if [[ ${lying} -eq 0 ]]; then
  ok "every NOTE is truthful"
fi

printf '\n%s==> counting%s\n' "${BOLD}" "${RESET}"
count="$(find "${ROOT}/exercises" -mindepth 2 -maxdepth 2 -type d | wc -l | tr -d ' ')"
solution_count="$(find "${ROOT}/solutions" -mindepth 2 -maxdepth 2 -type d | wc -l | tr -d ' ')"
printf '  %s exercises, %s solutions\n' "${count}" "${solution_count}"
if [[ "${count}" != "${solution_count}" ]]; then
  note "every exercise needs a solution"
fi

printf '\n'
if [[ ${failures} -eq 0 ]]; then
  printf '%s%sthe course is consistent%s\n' "${GREEN}" "${BOLD}" "${RESET}"
  exit 0
fi
printf '%s%s%d problem(s)%s\n' "${RED}" "${BOLD}" "${failures}" "${RESET}"
exit 1
