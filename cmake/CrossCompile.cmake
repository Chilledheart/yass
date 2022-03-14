include(ExternalProjectUtils)

function(create_cross_target project_name target_name toolchain buildtype)
  if(NOT DEFINED ${project_name}_${target_name}_BUILD)
    set(${project_name}_${target_name}_BUILD
      "${CMAKE_CURRENT_BINARY_DIR}/${target_name}")
    set(${project_name}_${target_name}_BUILD
      ${${project_name}_${target_name}_BUILD} PARENT_SCOPE)
    message(STATUS "Setting cross build dir to " ${${project_name}_${target_name}_BUILD})
  endif(NOT DEFINED ${project_name}_${target_name}_BUILD)

  if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${toolchain}.cmake)
    set(CROSS_TOOLCHAIN_FLAGS_INIT
      -DCMAKE_TOOLCHAIN_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/cmake/${toolchain}.cmake\")
  else()
    set(CROSS_TOOLCHAIN_FLAGS_INIT
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      )
  endif()
  set(CROSS_TOOLCHAIN_FLAGS_${target_name} ${CROSS_TOOLCHAIN_FLAGS_INIT}
    CACHE STRING "Toolchain configuration for ${target_name}")

  # project specific version of the flags up above
  set(CROSS_TOOLCHAIN_FLAGS_${project_name}_${target_name} ""
    CACHE STRING "Toolchain configuration for ${Pproject_name}_${target_name}")

  if (buildtype)
    set(build_type_flags "-DCMAKE_BUILD_TYPE=${buildtype}")
  endif()

  if (ALLOW_XP)
    set(allow_xp_flags "-DALLOW_XP=on")
  endif()

  set(use_libcxx_flags "-DUSE_LIBCXX=${USE_LIBCXX}")
  set(use_cli_flags "-DCLI=${CLI}")
  set(use_server_flags "-DSERVER=${SERVER}")
  set(use_gui_flags "-DGUI=${GUI}")
  set(use_build_tests_flags "-DBUILD_TESTS=${BUILD_TESTS}")

  set(osx_deployment_flags "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  set(osx_architectures_flags "-DCMAKE_OSX_ARCHITECTURES=\"${CMAKE_OSX_ARCHITECTURES}\"")

  add_custom_command(OUTPUT ${${project_name}_${target_name}_BUILD}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${${project_name}_${target_name}_BUILD}
    COMMENT "Creating ${${project_name}_${target_name}_BUILD}...")

  add_custom_target(CREATE_${project_name}_${target_name}
    DEPENDS ${${project_name}_${target_name}_BUILD})

  add_custom_command(OUTPUT ${${project_name}_${target_name}_BUILD}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E env "LIB=${CROSS_TOOLCHAIN_FLAGS_${target_name}_LIB}" ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
        -DYASS_TARGET_IS_CROSSCOMPILE_HOST=TRUE
        -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
        -DCMAKE_MAKE_PROGRAM="${CMAKE_MAKE_PROGRAM}"
        ${CROSS_TOOLCHAIN_FLAGS_${target_name}} ${CMAKE_CURRENT_SOURCE_DIR}
        ${CROSS_TOOLCHAIN_FLAGS_${project_name}_${target_name}}
        ${build_type_flags} ${linker_flag} ${allow_xp_flags} ${use_libcxx_flags}
        ${use_cli_flags} ${use_server_flags} ${use_gui_flags} ${use_build_tests_flags}
        ${osx_deployment_flags} ${osx_architectures_flags}
        ${ARGN}
    WORKING_DIRECTORY ${${project_name}_${target_name}_BUILD}
    DEPENDS CREATE_${project_name}_${target_name}
    COMMENT "Configuring ${target_name} ${project_name}...")

  add_custom_target(CONFIGURE_${project_name}_${target_name}
    DEPENDS ${${project_name}_${target_name}_BUILD}/CMakeCache.txt)

endfunction()

# Sets up a native build for a tool, used e.g. for cross-compilation and
# OPTIMIZED_PROTOC. Always builds in Release.
# - target: The target to build natively
# - output_path_var: A variable name which receives the path to the built target
# - DEPENDS: Any additional dependencies for the target
function(build_native_tool target output_path_var)
  cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

  if(CMAKE_CONFIGURATION_TYPES)
    set(output_path "${${PROJECT_NAME}_NATIVE_BUILD}/Release/bin/${target}")
  else()
    set(output_path "${${PROJECT_NAME}_NATIVE_BUILD}/bin/${target}")
  endif()
  set(output_path ${output_path}${YASS_HOST_EXECUTABLE_SUFFIX})

  yass_ExternalProject_BuildCmd(build_cmd ${target} ${${PROJECT_NAME}_NATIVE_BUILD}
                                CONFIGURATION Release)
  add_custom_command(OUTPUT "${output_path}"
                     COMMAND ${CMAKE_COMMAND} -E env "LIB=${CROSS_TOOLCHAIN_FLAGS_NATIVE_LIB}" ${build_cmd}
                     DEPENDS CONFIGURE_${PROJECT_NAME}_NATIVE ${ARG_DEPENDS}
                     WORKING_DIRECTORY "${${PROJECT_NAME}_NATIVE_BUILD}"
                     COMMENT "Building native ${target}...")
  set(${output_path_var} "${output_path}" PARENT_SCOPE)
endfunction()

# Sets up a sub build for multi arch builds
# - target: The target to build
# - arch: The arch to build
# - output_path_var: A variable name which receives the path to the built target
# - DEPENDS: Any additional dependencies for the target
function(build_osx_arch target arch output_path_var)
  cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

  if(CMAKE_CONFIGURATION_TYPES)
    set(output_path "${${PROJECT_NAME}_OSX_${arch}_BUILD}/${CMAKE_CONFIGURATION_TYPES}/${target}")
  else()
    set(output_path "${${PROJECT_NAME}_OSX_${arch}_BUILD}/${target}")
  endif()

  set(output_path ${output_path}${YASS_HOST_EXECUTABLE_SUFFIX})

  yass_ExternalProject_BuildCmd(build_cmd ${target} ${${PROJECT_NAME}_OSX_${arch}_BUILD}
                                CONFIGURATION ${CMAKE_CONFIGURATION_TYPES})

  add_custom_command(OUTPUT "${output_path}"
                     COMMAND ${build_cmd}
                     DEPENDS CONFIGURE_${PROJECT_NAME}_OSX_${arch} ${ARG_DEPENDS}
                     WORKING_DIRECTORY "${${PROJECT_NAME}_OSX_${arch}_BUILD}"
                     COMMENT "Building osx ${target} arch ${arch} ...")
  set(${output_path_var} "${output_path}" PARENT_SCOPE)
endfunction()
