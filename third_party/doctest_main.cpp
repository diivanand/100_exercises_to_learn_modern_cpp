// The single translation unit that instantiates doctest's implementation and
// its `main`. Every exercise links against the static library built from this
// file, so the (fairly large) header body is compiled once for the whole
// project rather than once per exercise.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
