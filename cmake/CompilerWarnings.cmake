# Warning set adapted from cpp-best-practices/cppbestpractices, chapter 2:
# https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md
#
# The compiler is the cheapest static analyser you will ever run. Turning
# these on -- and turning them into errors -- is the single highest-value
# thing you can do to a C++ build.

function(mcpp_set_project_warnings target as_errors)
  set(clang_warnings
      -Wall
      -Wextra # reasonable and standard
      -Wshadow # a variable declaration shadows one from a parent context
      -Wnon-virtual-dtor # a class with virtual functions has a non-virtual dtor
      -Wold-style-cast # C-style casts
      -Wcast-align # potentially performance-harmful casts
      -Wunused # anything unused
      -Woverloaded-virtual # a derived function *hides* a base virtual function
      -Wpedantic # non-standard C++ is used
      -Wconversion # type conversions that may lose data
      -Wsign-conversion # sign conversions that may lose data
      -Wnull-dereference
      -Wdouble-promotion # float implicitly promoted to double
      -Wformat=2 # security issues around printf-family formatting
      -Wimplicit-fallthrough # a switch case falls through without [[fallthrough]]
  )

  set(gcc_warnings
      ${clang_warnings}
      -Wmisleading-indentation
      -Wduplicated-cond
      -Wduplicated-branches
      -Wlogical-op
      -Wuseless-cast)

  set(msvc_warnings /W4 /permissive-)

  if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(warnings ${clang_warnings})
    if(as_errors)
      list(APPEND warnings -Werror)
    endif()
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(warnings ${gcc_warnings})
    if(as_errors)
      list(APPEND warnings -Werror)
    endif()
  elseif(MSVC)
    set(warnings ${msvc_warnings})
    if(as_errors)
      list(APPEND warnings /WX)
    endif()
  endif()

  target_compile_options(${target} INTERFACE ${warnings})
endfunction()
