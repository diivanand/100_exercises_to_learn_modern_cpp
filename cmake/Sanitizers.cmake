# Runtime sanitizers catch the bugs the type system cannot: use-after-free,
# buffer overruns, signed overflow, data races. They cost ~2x runtime and are
# worth running over the whole test suite before you trust any code.
#
#   cmake --preset asan   && ctest --preset asan     # address + undefined
#   cmake --preset tsan   && ctest --preset tsan     # threads (chapter 10)
#
# ASan and TSan are mutually exclusive: they both instrument memory and their
# runtimes conflict. That is why this is a single-choice variable, not a set
# of independent booleans.

set(MCPP_SANITIZER
    "none"
    CACHE STRING "Runtime sanitizer: none | address | thread | memory")
set_property(CACHE MCPP_SANITIZER PROPERTY STRINGS none address thread memory)

function(mcpp_enable_sanitizers target)
  if(MCPP_SANITIZER STREQUAL "none")
    return()
  endif()

  if(NOT (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang" OR CMAKE_CXX_COMPILER_ID
                                                     STREQUAL "GNU"))
    message(WARNING "MCPP_SANITIZER is only wired up for Clang and GCC")
    return()
  endif()

  if(MCPP_SANITIZER STREQUAL "address")
    # UBSan composes with ASan, and the pair is the standard debug build.
    set(flags -fsanitize=address,undefined -fno-omit-frame-pointer
              -fno-sanitize-recover=undefined)
  elseif(MCPP_SANITIZER STREQUAL "thread")
    set(flags -fsanitize=thread)
  elseif(MCPP_SANITIZER STREQUAL "memory")
    set(flags -fsanitize=memory -fno-omit-frame-pointer)
  else()
    message(FATAL_ERROR "Unknown MCPP_SANITIZER value: ${MCPP_SANITIZER}")
  endif()

  target_compile_options(${target} INTERFACE ${flags} -g)
  target_link_options(${target} INTERFACE ${flags})
endfunction()
