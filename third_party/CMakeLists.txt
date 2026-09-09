# doctest is vendored as a single header (third_party/doctest/doctest.h) so the
# repository builds with no network access. See third_party/doctest/README.md.
add_library(mcpp_doctest INTERFACE)
add_library(mcpp::doctest ALIAS mcpp_doctest)

# SYSTEM stops our aggressive warning set from firing inside doctest's own
# headers -- you are responsible for warnings in *your* code, not in a
# dependency's.
target_include_directories(mcpp_doctest SYSTEM
                           INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

# SYSTEM is not quite enough. doctest's TEST_CASE macro expands to a use of
# `__COUNTER__`, and a macro-expansion diagnostic is reported at the EXPANSION
# site -- which is the exercise file, not the header. Clang 20 and later flag
# `__COUNTER__` under -Wpedantic as a C2y extension, so an otherwise clean
# exercise fails to build on a newer toolchain than the one it was written on.
#
# The suppression belongs here, on the dependency's own interface target, so it
# travels with doctest to whatever links it -- rather than being pasted into
# the project-wide warning set, where it would also hide the same construct in
# code we actually own.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wno-c2y-extensions MCPP_HAS_WNO_C2Y_EXTENSIONS)
if(MCPP_HAS_WNO_C2Y_EXTENSIONS)
  target_compile_options(mcpp_doctest INTERFACE -Wno-c2y-extensions)
endif()

# doctest's `main` is compiled exactly once, here, instead of once per
# exercise. With ~100 executables that is a meaningful chunk of build time.
add_library(mcpp_doctest_main STATIC doctest_main.cpp)
add_library(mcpp::doctest_main ALIAS mcpp_doctest_main)
target_link_libraries(mcpp_doctest_main PUBLIC mcpp::doctest)
target_compile_features(mcpp_doctest_main PUBLIC cxx_std_20)
