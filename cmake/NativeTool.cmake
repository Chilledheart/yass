macro(add_native_protoc target project)
  # CMake doesn't let compilation units depend on their dependent libraries on some generators.

  set(${project}_PROTOC "${target}" CACHE
      STRING "Native Tool executable. Saves building one when cross-compiling.")

  # Effective native tool executable to be used:
  set(${project}_PROTOC_EXE ${${project}_PROTOC})
  set(${project}_PROTOC_TARGET ${${project}_PROTOC})

  if(USE_HOST_TOOLS)
    if( ${${project}_PROTOC} STREQUAL "${target}" )
      # The NATIVE tablegen executable *must* depend on the current target one
      # otherwise the native one won't get rebuilt when the tablgen sources
      # change, and we end up with incorrect builds.
      build_native_tool(${target} ${project}_PROTOC_EXE)
      set(${project}_PROTOC_EXE ${${project}_PROTOC_EXE})

      add_custom_target(${project}-native-tool-host DEPENDS ${${project}_PROTOC_EXE})
      set(${project}_PROTOC_TARGET ${project}-protoc-host)

      # Create an artificial dependency between protoc projects, because they
      # compile the same dependencies, thus using the same build folders.
      # FIXME: A proper fix requires sequentially chaining protocs.
      if (NOT ${project} STREQUAL yass AND TARGET ${project}-protoc-host AND
          TARGET yass-protoc-host)
        add_dependencies(${project}-protoc-host yass-protoc-host)
      endif()
    endif()
  endif()
endmacro()

