cmake_minimum_required(VERSION 3.24)

project(
  modern_cpp_exercises
  VERSION 1.0.0
  DESCRIPTION "100 exercises to learn modern C++ (C++20), plus a CUDA track"
  LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Language level.
#
# We ask for C++20 and refuse to silently fall back to an older standard
# (CXX_STANDARD_REQUIRED) or to enable vendor extensions such as GNU's
# `typeof` (CXX_EXTENSIONS OFF -> -std=c++20 instead of -std=gnu++20).
# Portable code should never depend on extensions it did not ask for.
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json so clang-tidy, clangd and CLion all see the exact
# flags each translation unit is compiled with.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)
include(Sanitizers)
include(AddExercise)

option(MCPP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(MCPP_BUILD_EXERCISES "Build the exercise targets (your work)" ON)
option(MCPP_BUILD_SOLUTIONS "Build the reference solutions" OFF)

# ---------------------------------------------------------------------------
# Two interface targets carry every setting the exercises share.
#
# This is the modern CMake idiom: no global `add_compile_options`, no
# directory-scoped state leaking into subdirectories. A target says what it
# needs, and consumers inherit it through `target_link_libraries`.
# ---------------------------------------------------------------------------
add_library(mcpp_options INTERFACE)
add_library(mcpp::options ALIAS mcpp_options)
target_compile_features(mcpp_options INTERFACE cxx_std_20)

add_library(mcpp_warnings INTERFACE)
add_library(mcpp::warnings ALIAS mcpp_warnings)
mcpp_set_project_warnings(mcpp_warnings ${MCPP_WARNINGS_AS_ERRORS})

mcpp_enable_sanitizers(mcpp_options)

# Chapters 14-17 need nvcc and a GPU. This detects CUDA and sets up the
# `mcpp::cuda` target when it is there, and skips those chapters when it is
# not. See cmake/Cuda.cmake.
include(Cuda)

add_subdirectory(third_party)

enable_testing()

if(MCPP_BUILD_EXERCISES)
  add_subdirectory(exercises)
endif()

if(MCPP_BUILD_SOLUTIONS)
  add_subdirectory(solutions)
endif()

message(STATUS "")
message(STATUS "  modern C++ exercises")
message(STATUS "  compiler ............ ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  build type .......... ${CMAKE_BUILD_TYPE}")
message(STATUS "  warnings as errors .. ${MCPP_WARNINGS_AS_ERRORS}")
message(STATUS "  exercises ........... ${MCPP_BUILD_EXERCISES}")
message(STATUS "  solutions ........... ${MCPP_BUILD_SOLUTIONS}")
message(STATUS "  sanitizers .......... ${MCPP_SANITIZER}")
message(STATUS "  CUDA chapters ....... ${MCPP_CUDA_ENABLED}")
message(STATUS "")
