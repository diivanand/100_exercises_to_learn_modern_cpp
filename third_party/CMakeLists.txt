# doctest is vendored as a single header (third_party/doctest/doctest.h) so the
# repository builds with no network access. See third_party/doctest/README.md.
add_library(mcpp_doctest INTERFACE)
add_library(mcpp::doctest ALIAS mcpp_doctest)

# SYSTEM stops our aggressive warning set from firing inside doctest's own
# headers -- you are responsible for warnings in *your* code, not in a
# dependency's.
target_include_directories(mcpp_doctest SYSTEM
                           INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

# doctest's `main` is compiled exactly once, here, instead of once per
# exercise. With ~100 executables that is a meaningful chunk of build time.
add_library(mcpp_doctest_main STATIC doctest_main.cpp)
add_library(mcpp::doctest_main ALIAS mcpp_doctest_main)
target_link_libraries(mcpp_doctest_main PUBLIC mcpp::doctest)
target_compile_features(mcpp_doctest_main PUBLIC cxx_std_20)
