# doctest (vendored)

- **Version:** 2.4.11
- **Upstream:** <https://github.com/doctest/doctest>
- **License:** MIT (see `LICENSE.txt`)

`doctest.h` is checked in verbatim so that this repository configures and
builds with no network access. Do not edit it by hand; to upgrade, replace the
file with a newer single-header release and update the version above.

Why doctest rather than Catch2 or GoogleTest for a teaching repository:

- one header, zero build configuration, no package manager required;
- assertion macros (`CHECK`, `REQUIRE`, `CHECK_THROWS_AS`) that read the same
  way as Catch2's, so the habits transfer;
- fast to compile, which matters when the project holds ~100 test binaries.
