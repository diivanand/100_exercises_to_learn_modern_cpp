# Exercise discovery.
#
# Every exercise lives in `exercises/<NN_chapter>/<NN_name>/exercise.cpp` and
# is a self-contained doctest translation unit. Rather than maintaining 100
# hand-written `add_executable` calls, we glob the tree and derive everything
# from the directory layout: add a directory, get a target and a test.
#
# `CONFIGURE_DEPENDS` makes the build system re-run the glob when the set of
# matching files changes. Globbing is often discouraged (CMake cannot know
# about a new file without re-running), but for a fixed, regular layout like
# this one it removes far more boilerplate than it costs.

function(mcpp_discover_exercises kind)
  if(kind STREQUAL "exercise")
    set(prefix "ex")
    set(root "${CMAKE_CURRENT_SOURCE_DIR}")
  elseif(kind STREQUAL "solution")
    set(prefix "sol")
    set(root "${CMAKE_CURRENT_SOURCE_DIR}")
  else()
    message(FATAL_ERROR "mcpp_discover_exercises: kind must be exercise|solution")
  endif()

  file(
    GLOB_RECURSE sources
    CONFIGURE_DEPENDS
    "${root}/*/*/exercise.cpp")
  list(SORT sources)

  if(NOT sources)
    message(WARNING "No ${kind} sources found under ${root}")
    return()
  endif()

  set(all_targets "")
  foreach(source IN LISTS sources)
    # exercises/03_ownership/05_move/exercise.cpp -> "03_ownership/05_move"
    file(RELATIVE_PATH relative "${root}" "${source}")
    get_filename_component(dir "${relative}" DIRECTORY)

    string(REPLACE "/" ";" parts "${dir}")
    list(GET parts 0 chapter) # 03_ownership
    list(GET parts 1 name) # 05_move

    # "03_ownership" -> "03", "05_move" -> "05_move"
    string(REGEX REPLACE "^([0-9]+)_.*$" "\\1" chapter_number "${chapter}")

    set(test_name "${chapter_number}_${name}") # 03_05_move
    set(target "${prefix}_${test_name}") # ex_03_05_move

    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE mcpp::options mcpp::warnings
                                            mcpp::doctest_main)

    # Keep the binaries grouped so `ls` in the build tree is readable, and so
    # IDEs show the same folder structure as the source tree.
    set_target_properties(
      ${target}
      PROPERTIES FOLDER "${kind}s/${chapter}"
                 RUNTIME_OUTPUT_DIRECTORY
                 "${CMAKE_BINARY_DIR}/bin/${kind}s/${chapter}")

    if(kind STREQUAL "exercise")
      add_test(NAME "${test_name}" COMMAND ${target})
    else()
      add_test(NAME "solution/${test_name}" COMMAND ${target})
    endif()

    list(APPEND all_targets ${target})
  endforeach()

  list(LENGTH all_targets count)
  message(STATUS "Discovered ${count} ${kind}s")

  # A convenience target so `cmake --build build --target exercises` builds
  # everything in one go without also building the solutions.
  add_custom_target(${kind}s DEPENDS ${all_targets})
endfunction()
