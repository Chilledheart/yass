include(ExternalProject)

# ExternalProject_BuildCmd(out_var target)
#   Utility function for constructing command lines for external project targets
function(yass_ExternalProject_BuildCmd out_var target bin_dir)
  cmake_parse_arguments(ARG "" "CONFIGURATION" "" ${ARGN})
  if(NOT ARG_CONFIGURATION)
    set(ARG_CONFIGURATION "$<CONFIG>")
  endif()
  if (CMAKE_GENERATOR MATCHES "Make")
    # Use special command for Makefiles to support parallelism.
    set(${out_var} "$(MAKE)" "-C" "${bin_dir}" "${target}" PARENT_SCOPE)
  else()
    set(${out_var} ${CMAKE_COMMAND} --build ${bin_dir} --target ${target}
                                    --config ${ARG_CONFIGURATION} PARENT_SCOPE)
  endif()
endfunction()
