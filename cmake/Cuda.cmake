# CUDA support for chapters 14 to 17.
#
# The CUDA chapters are written in the same shape as the rest of the course --
# one self-contained doctest file per exercise -- but the file is `exercise.cu`
# and it needs nvcc and an NVIDIA GPU. Most people write the code on a laptop
# without either, and build and run it on a Linux machine that has both (the
# README describes the CLion remote-development setup for exactly that). So:
#
#   * CUDA is detected, never required. If nvcc is not found the CUDA
#     exercises are skipped with a message, and everything else builds.
#   * -DMCPP_ENABLE_CUDA=OFF turns the detection off; =ON makes a missing
#     nvcc a hard error, which is what you want on the GPU machine so that a
#     PATH problem cannot silently turn into "0 CUDA exercises discovered".
#
# What it sets up when CUDA is available: the language, C++20 for device
# code, the architecture (Ada Lovelace, sm_89, an RTX 4090 -- override with
# -DCMAKE_CUDA_ARCHITECTURES=...), and an interface target `mcpp::cuda` that
# every CUDA exercise links to. That target carries the nvcc flags the course
# relies on:
#
#   --extended-lambda        a lambda marked __device__ can be passed to a
#                            kernel or to thrust (16.07)
#   --expt-relaxed-constexpr a constexpr function is callable from device code
#                            without also marking it __device__ (14.05)
#   -lineinfo                source-line information in the binary, so Nsight
#                            Compute can attribute stalls to YOUR lines (17.03).
#                            It costs nothing at run time. (This is not -G,
#                            which disables device optimisation and would make
#                            every performance exercise meaningless.)
#
# Warnings are handled separately from the C++ chapters. nvcc forwards host
# flags with -Xcompiler, and the CUDA headers -- thrust, cub, libcu++ -- are
# not clean under the aggressive set in CompilerWarnings.cmake, so `-Werror`
# would fail on code you do not own. CUDA targets get -Wall -Wextra on the
# host side and every nvcc warning on the device side, as warnings.

set(MCPP_ENABLE_CUDA
    "AUTO"
    CACHE STRING "Build the CUDA chapters: AUTO (if nvcc is found) | ON | OFF")
set_property(CACHE MCPP_ENABLE_CUDA PROPERTY STRINGS AUTO ON OFF)

set(MCPP_CUDA_ENABLED OFF)

if(MCPP_ENABLE_CUDA STREQUAL "OFF")
  message(STATUS "CUDA chapters: disabled (MCPP_ENABLE_CUDA=OFF)")
  return()
endif()

include(CheckLanguage)
check_language(CUDA)

if(NOT CMAKE_CUDA_COMPILER)
  if(MCPP_ENABLE_CUDA STREQUAL "ON")
    message(FATAL_ERROR "MCPP_ENABLE_CUDA=ON but no CUDA compiler was found. "
                        "Is nvcc on your PATH? (Try: export PATH=/usr/local/cuda/bin:$PATH)")
  endif()
  message(STATUS "CUDA chapters: skipped (no nvcc found -- fine on a laptop; "
                 "build them on the GPU machine)")
  return()
endif()

# An RTX 4090 is Ada Lovelace, compute capability 8.9. Set this before
# enable_language so CMake does not probe for a default.
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES 89)
endif()

enable_language(CUDA)
find_package(CUDAToolkit 12.4 REQUIRED)

set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_EXTENSIONS OFF)

add_library(mcpp_cuda INTERFACE)
add_library(mcpp::cuda ALIAS mcpp_cuda)
target_compile_features(mcpp_cuda INTERFACE cuda_std_20)
target_compile_options(
  mcpp_cuda
  INTERFACE $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
            $<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>
            $<$<COMPILE_LANGUAGE:CUDA>:-lineinfo>
            $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-Wall,-Wextra>
            # A __host__ function called from __device__ code is an error, not
            # a warning -- it is the mistake 14.05 is about.
            $<$<COMPILE_LANGUAGE:CUDA>:-Werror=cross-execution-space-call>)
target_link_libraries(mcpp_cuda INTERFACE CUDA::cudart)

# NVTX ranges (17.02) name regions of your program on the Nsight Systems
# timeline. Since NVTX 3 it is header-only; CMake 3.25+ models it as a target.
if(TARGET CUDA::nvtx3)
  target_link_libraries(mcpp_cuda INTERFACE CUDA::nvtx3)
else()
  target_include_directories(mcpp_cuda SYSTEM INTERFACE "${CUDAToolkit_INCLUDE_DIRS}")
  target_link_libraries(mcpp_cuda INTERFACE ${CMAKE_DL_LIBS})
endif()

set(MCPP_CUDA_ENABLED ON)
# Written to the cache so that ./mcpp can read it without re-running CMake.
set(MCPP_CUDA_ENABLED
    ON
    CACHE INTERNAL "CUDA chapters are being built")
message(STATUS "CUDA chapters: enabled (nvcc ${CMAKE_CUDA_COMPILER_VERSION}, "
               "sm_${CMAKE_CUDA_ARCHITECTURES})")
