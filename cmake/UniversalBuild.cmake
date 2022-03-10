macro(add_osx_univeral_arch target arch project)
  # CMake doesn't let compilation units depend on their dependent libraries on some generators.

  set(${project}_${target}_${arch} "${target}" CACHE
      STRING "arched target. Saves building one when cross-compiling.")

  # Effective arched target to be used:
  set(${project}_${target}_${arch}_EXE ${${project}_${target}_${arch}})
  set(${project}_${target}_${arch}_TARGET ${${project}_${target}_${arch}})

  if (OSX_UNIVERSALBUILD)
    if (${${project}_${target}_${arch}} STREQUAL "${target}")
      # The cross target *must* depend on the current target one
      # otherwise the native one won't get rebuilt when the tablgen sources
      # change, and we end up with incorrect builds.
      # build_osx_arch(${target} ${arch} ${project}_${target}_${arch}_EXE)
      build_osx_arch(${target} ${arch} ${project}_${target}_${arch}_EXE)
      set(${project}_${target}_${arch}_EXE ${${project}_${target}_${arch}_EXE})

      add_custom_target(${project}-${target}-${arch}-host DEPENDS ${${project}_${target}_${arch}_EXE})
      set(${project}_${target}_${arch}_TARGET ${project}-${target}-${arch}-host)

      # Create an artificial dependency between ${target}-${arch} projects, because they
      # compile the same dependencies, thus using the same build folders.
      # FIXME: A proper fix requires sequentially chaining ${target}-${arch}s.
      if (NOT ${project} STREQUAL yass AND TARGET ${project}-${target}-${arch}-host AND
          TARGET yass-${target}-${arch}-host)
        add_dependencies(${project}-${target}-${arch}-host yass-${target}-${arch}-host)
      endif()

      # If we're using the host ${target}-${arch}, and utils were not requested, we have no
      # need to build this ${target}-${arch}.
      # set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL ON)
    endif()
  endif()
endmacro()

macro(add_osx_univeral_target target arches project)
  foreach (arch ${arches})
    add_osx_univeral_arch(${target} ${arch} ${project})
    set(${target}_universal_EXE ${${target}_universal_EXE} "${${project}_${target}_${arch}_EXE}")
    set(${target}_universal_TARGET ${${target}_universal_TARGET} ${${project}_${target}_${arch}_TARGET})
  endforeach()
  set(${target}_universal_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/universal/${target}")
  add_custom_command(OUTPUT "${${target}_universal_OUTPUT}"
                     COMMAND ${CMAKE_COMMAND} -E make_directory universal
                     COMMAND lipo -create ${${target}_universal_EXE} -output ${${target}_universal_OUTPUT}
                     COMMAND ${CMAKE_COMMAND} -E copy ${${target}_universal_OUTPUT} ${target}
                     DEPENDS ${${target}_universal_EXE} ${${target}_universal_TARGET}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     COMMENT "Creating universal binary ${target}..."
                     USES_TERMINAL)
  set_source_files_properties("${${target}_universal_OUTPUT}" PROPERTIES GENERATED TRUE)
  add_custom_target(${target} DEPENDS "${${target}_universal_OUTPUT}")
endmacro()

macro(add_osx_univeral_bundle target arches project)
  foreach (arch ${arches})
    add_osx_univeral_arch(${target} ${arch} ${project})
    set(${target}_universal_EXE ${${target}_universal_EXE} "${${project}_${target}_${arch}_EXE}")
    set(${target}_universal_EXE_OUTPUT ${${target}_universal_EXE_OUTPUT} "${${project}_${target}_${arch}_EXE}.app/Contents/MacOS/${target}")
    set(${target}_universal_TARGET ${${target}_universal_TARGET} ${${project}_${target}_${arch}_TARGET})
  endforeach()
  set(${target}_native_EXE "${${project}_${target}_${CMAKE_SYSTEM_PROCESSOR}_EXE}.app")
  set(${target}_universal_EXE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/universal/${target}.app/Contents/MacOS/${target}")
  set(${target}_universal_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/universal/${target}.app")
  add_custom_command(OUTPUT "${${target}_universal_OUTPUT}"
                     COMMAND ${CMAKE_COMMAND} -E make_directory universal
                     COMMAND ${CMAKE_COMMAND} -E copy_directory ${${target}_native_EXE} ${${target}_universal_OUTPUT}
                     COMMAND lipo -create ${${target}_universal_EXE_OUTPUT} -output ${${target}_universal_EXE_OUTPUT}
                     COMMAND ${CMAKE_COMMAND} -E copy_directory ${${target}_universal_OUTPUT} ${target}.app
                     DEPENDS ${${target}_universal_EXE} ${${target}_universal_TARGET}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     COMMENT "Creating universal bundle ${target}..."
                     USES_TERMINAL)
  set_source_files_properties("${${target}_universal_OUTPUT}" PROPERTIES GENERATED TRUE)
  add_custom_target(${target} DEPENDS "${${target}_universal_OUTPUT}")
endmacro()
