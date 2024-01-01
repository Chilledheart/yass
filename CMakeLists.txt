cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

if (CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(FATAL_ERROR "In source-tree build not support")
endif()

# Compiler id for Apple Clang is now AppleClang.
# https://cmake.org/cmake/help/latest/policy/CMP0025.html
if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif (POLICY CMP0025)

# Project version variables are the empty string if version is unspecified
if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

# Include file check macros honor CMAKE_REQUIRED_LIBRARIES.
set(CMAKE_POLICY_DEFAULT_CMP0075 NEW)
if (POLICY CMP0075)
  cmake_policy(SET CMP0075 NEW)
endif (POLICY CMP0075)

# Select the MSVC runtime library for use by compilers targeting the MSVC ABI.
# Use MSVC_CRT_LINKAGE variable internally, option: dynamic and static
#
# Introduced by cmake 3.15
# https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html#variable:CMAKE_MSVC_RUNTIME_LIBRARY
# and https://cmake.org/cmake/help/latest/policy/CMP0091.html
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

# The OLD behavior of this policy is to only allow GENERATED to be
# visible from the directory scope for which it was set.  The NEW
# behavior on the other hand allows it to be visible from any scope.
#
# Turn on Fix on https://gitlab.kitware.com/cmake/cmake/-/issues/18399
# Introduced by cmake 3.20
if(POLICY CMP0118)
  cmake_policy(SET CMP0118 NEW)
endif()

# https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?redirectedfrom=MSDN&view=msvc-170
set(MSVC_CRT_LINKAGE "static" CACHE STRING "")
if (NOT MSVC_CRT_LINKAGE STREQUAL "static" AND NOT MSVC_CRT_LINKAGE STREQUAL "dynamic")
  message(FATAL_ERROR "MSVC_CRT_LINKAGE only accepts: dynamic or static")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:${MSVC_CRT_LINKAGE},dynamic>:DLL>")

# Use relative paths (May not work!).
set(CMAKE_USE_RELATIVE_PATHS TRUE)

# update LAST_CHANGE file
# format d740da257583289dbebd2eb37e8668928fac5ead-refs/branch-heads/4692{32}
find_program(GIT_EXECUTABLE NAMES git git.exe git.cmd)
if(NOT GIT_EXECUTABLE STREQUAL "GIT_EXECUTABLE-NOTFOUND" AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
  execute_process(COMMAND
    ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE _LAST_CHANGE_REF
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND
    ${GIT_EXECUTABLE} describe --abbrev=0 --tags HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE YASS_APP_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

  execute_process(COMMAND
    ${GIT_EXECUTABLE} rev-list ${YASS_APP_TAG}..HEAD --count
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE YASS_APP_SUBTAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  set(_ABBREV_REF "${YASS_APP_TAG}\{${YASS_APP_SUBTAG}\}")

  set(YASS_APP_LAST_CHANGE "${_LAST_CHANGE_REF}-refs/branch-heads/${_ABBREV_REF}")
else()
  # read if running from tarball
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/LAST_CHANGE YASS_APP_LAST_CHANGE)
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/TAG YASS_APP_TAG)
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/SUBTAG YASS_APP_SUBTAG)
endif()

set(_YASS_VERSION ${YASS_APP_TAG})

set(_YASS_VERSION_MAJOR     1)
set(_YASS_VERSION_MINOR     3)
set(_YASS_VERSION_PATCH     0)
set(_YASS_VERSION_TWEAK     0)
string(REPLACE "-" "." _YASS_VERSION_SPLIT ${_YASS_VERSION})
string(REPLACE "." ";" _YASS_VERSION_SPLIT ${_YASS_VERSION_SPLIT})
list(GET _YASS_VERSION_SPLIT 0 _YASS_VERSION_MAJOR)
list(GET _YASS_VERSION_SPLIT 1 _YASS_VERSION_MINOR)
list(GET _YASS_VERSION_SPLIT 2 _YASS_VERSION_PATCH)
set(_YASS_VERSION_TWEAK     ${YASS_APP_SUBTAG})

set(PACKAGE_NAME            "yass")
set(PACKAGE_FULL_NAME       "Yet Another Shadow Socket")
set(PACKAGE_VERSION_MAJOR ${_YASS_VERSION_MAJOR})
set(PACKAGE_VERSION_MINOR ${_YASS_VERSION_MINOR})
set(PACKAGE_VERSION_PATCH ${_YASS_VERSION_PATCH})
set(PACKAGE_VERSION_TWEAK ${_YASS_VERSION_TWEAK})
set(PACKAGE_VERSION ${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_PATCH})

# Defer enabling C and CXX languages.
project(${PACKAGE_NAME} NONE)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Set variables used in configure_file
set(YASS_APP_NAME ${PACKAGE_NAME})
set(YASS_APP_FULL_NAME ${PACKAGE_FULL_NAME})
set(YASS_APP_INTERNAL_NAME ${PACKAGE_NAME}_gui)
set(YASS_APP_WEBSITE "https://github.com/Chilledheart/yass")
set(YASS_APP_VERSION ${PACKAGE_VERSION})
set(YASS_APP_VERSION_MAJOR ${PACKAGE_VERSION_MAJOR})
set(YASS_APP_VERSION_MINOR ${PACKAGE_VERSION_MINOR})
set(YASS_APP_VERSION_PATCH ${PACKAGE_VERSION_PATCH})
set(YASS_APP_VERSION_TWEAK ${PACKAGE_VERSION_TWEAK})
set(YASS_APP_FULL_VERSION ${PACKAGE_VERSION}.${PACKAGE_VERSION_TWEAK})
set(YASS_APP_COPYRIGHT "Copyright 2019-2024 Chilledheart. All rights reserved.")

message(STATUS "Source Tag: ${YASS_APP_TAG}-${YASS_APP_SUBTAG}")
message(STATUS "Source LastChange: ${YASS_APP_LAST_CHANGE}")

set(APP_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/version.h")
configure_file("src/version.h.in" "${APP_VERSION_HEADER}" @ONLY)
# for version.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(YASS_APP_FEATURES)

if(MSVC)
  # On Windows, prefer cl over gcc if both are available. By default most of
  # the CMake generators prefer gcc, even on Windows.
  set(CMAKE_GENERATOR_CC cl)

  # new in cmake 3.10
  # https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ARCHITECTURE_ID.html
  # not used in ABI-detection due to the cmake issue https://gitlab.kitware.com/cmake/cmake/-/issues/17702
  set(CMAKE_C_COMPILER_ARCHITECTURE_ID "${CMAKE_C_COMPILER_TARGET}" CACHE STRING "")
  set(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "${CMAKE_CXX_COMPILER_TARGET}" CACHE STRING "")
endif()

if (APPLE AND NOT IOS AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14")
endif()

if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES ${CMAKE_SYSTEM_PROCESSOR})
endif()

enable_language(C)
enable_language(CXX)

# required by zstd
if (NOT COMPILER_MSVC)
  enable_language(ASM)
endif()

## Setup compiler informations
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(COMPILER_CLANG TRUE)
else()
  set(COMPILER_CLANG FALSE)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(COMPILER_APPLE_CLANG TRUE)
endif()

if (${CMAKE_GENERATOR} MATCHES "^Xcode.*")
  set(COMPILER_APPLE_CLANG TRUE)
endif()

if (COMPILER_CLANG AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 5.0)
  message(FATAL_ERROR "Requires Clang 5.0 or newer to compile")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(COMPILER_GCC TRUE)
else()
  set(COMPILER_GCC FALSE)
endif()

if (COMPILER_GCC AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.1)
  message(FATAL_ERROR "Requires GCC 7.1 or newer to compile")
endif()

if (MSVC AND NOT COMPILER_CLANG)
  set(COMPILER_MSVC TRUE)
else()
  set(COMPILER_MSVC FALSE)
endif()

if (COMPILER_MSVC)
  message(FATAL_ERROR "Only clang-cl is supported on Windows, see https://crbug.com/988071")
endif()

if((${CMAKE_LINKER} MATCHES "lld") AND (${CMAKE_AR} MATCHES "llvm-lib" OR ${CMAKE_AR} MATCHES "llvm-ar"))
  set(LINKER_LLD TRUE)
else()
  set(LINKER_LLD FALSE)
endif()

if (MSVC)
  if("${MSVC_C_ARCHITECTURE_ID}" STREQUAL "X86")
    set(MSVC_PROCESSOR_ARCHITECTURE "x86")
  elseif("${MSVC_C_ARCHITECTURE_ID}" STREQUAL "x64")
    set(MSVC_PROCESSOR_ARCHITECTURE "amd64")
  elseif("${MSVC_C_ARCHITECTURE_ID}" STREQUAL "ARM")
    set(MSVC_PROCESSOR_ARCHITECTURE "arm")
  elseif("${MSVC_C_ARCHITECTURE_ID}" STREQUAL "ARM64")
    set(MSVC_PROCESSOR_ARCHITECTURE "arm64")
  else()
    message(FATAL_ERROR "Failed to determine the MSVC target architecture: ${MSVC_C_ARCHITECTURE_ID}")
  endif()
endif()

if (IOS)
  list(APPEND YASS_APP_FEATURES "${SDK_NAME} ${DEPLOYMENT_TARGET}")
elseif(APPLE)
  list(APPEND YASS_APP_FEATURES "mac ${CMAKE_OSX_DEPLOYMENT_TARGET}")
elseif(OHOS)
  list(APPEND YASS_APP_FEATURES "ohos ${OHOS_APILEVEL}")
elseif(ANDROID)
  list(APPEND YASS_APP_FEATURES "android ${ANDROID_API_VERSION}")
elseif(MSVC)
  list(APPEND YASS_APP_FEATURES "msvc")
elseif(MINGW)
  list(APPEND YASS_APP_FEATURES "mingw")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND YASS_APP_FEATURES "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  if (NOT CMAKE_CROSSCOMPILING)
    execute_process(COMMAND
      uname -r
      OUTPUT_VARIABLE _ABI_VERSION_STRING
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX REPLACE "([0-9]+)\.[0-9]-.*" "\\1" FREEBSD_ABI_VERSION "${_ABI_VERSION_STRING}")
  endif()

  if (FREEBSD_ABI_VERSION)
    list(APPEND YASS_APP_FEATURES "freebsd ${FREEBSD_ABI_VERSION}")
  else()
    list(APPEND YASS_APP_FEATURES "freebsd")
  endif()
else()
  list(APPEND YASS_APP_FEATURES "system ${CMAKE_SYSTEM_NAME}")
endif()

if (MSVC)
  message(STATUS "Build with MSVC target architecture: ${MSVC_PROCESSOR_ARCHITECTURE}")
  message(STATUS "Build with MSVC ${MSVC_CRT_LINKAGE} CRT library")
  if (MSVC_PROCESSOR_ARCHITECTURE STREQUAL "x86")
    set(OS_X86 TRUE)
  endif()
  if (MSVC_PROCESSOR_ARCHITECTURE STREQUAL "arm64")
    set(OS_AARCH64 TRUE)
  endif()
  list(APPEND YASS_APP_FEATURES "arch: ${MSVC_PROCESSOR_ARCHITECTURE}")
  list(APPEND YASS_APP_FEATURES "${MSVC_CRT_LINKAGE} crt")
elseif(APPLE)
  message(STATUS "Build with target architecture: ${CMAKE_OSX_ARCHITECTURES}")
  list(APPEND YASS_APP_FEATURES "arch: ${CMAKE_OSX_ARCHITECTURES}")
else()
  if ((CMAKE_C_COMPILER_TARGET MATCHES "^i386-.*" OR
       CMAKE_C_COMPILER_TARGET MATCHES "^i486-.*" OR
       CMAKE_C_COMPILER_TARGET MATCHES "^i586-.*" OR
       CMAKE_C_COMPILER_TARGET MATCHES "^i686-.*"))
    set(OS_X86 TRUE)
  endif()
  if ((CMAKE_C_COMPILER_TARGET MATCHES "^arm64-.*" OR
       CMAKE_C_COMPILER_TARGET MATCHES "^aarch64-.*"))
    set(OS_AARCH64 TRUE)
  endif()
  # Fix MINGW (native mingw)'s CMAKE_SYSTEM_PROCESSOR
  if (MINGW)
    if (OS_X86)
      set(CMAKE_SYSTEM_NAME Windows)
      set(CMAKE_SYSTEM_PROCESSOR "i686")
    elseif(OS_AARCH64)
      set(CMAKE_SYSTEM_NAME Windows)
      set(CMAKE_SYSTEM_PROCESSOR "aarch64")
    else()
      set(CMAKE_SYSTEM_NAME Windows)
      set(CMAKE_SYSTEM_PROCESSOR "x86_64")
    endif()
  endif()

  message(STATUS "Build with target architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  list(APPEND YASS_APP_FEATURES "arch: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if (OS_X86)
  list(APPEND YASS_APP_FEATURES "x86")
endif()

# handling bug with cmake+clang-cl
# newer clang-cl produces "Note: including file: .\\foo.h"
# unable to parse by cmake's CMakeDetermineCompilerId.cmake
if (MSVC AND COMPILER_CLANG)
  set(CMAKE_CL_SHOWINCLUDES_PREFIX "Note: including file:" CACHE STRING "" FORCE)
endif()

## postpone ASM enabling after C/CXX LANG DIALECT otherwise it might fails
if (NOT (WIN32 AND (MSVC_PROCESSOR_ARCHITECTURE STREQUAL "x86" OR MSVC_PROCESSOR_ARCHITECTURE STREQUAL "amd64")))
  enable_language(ASM)
endif()

## Export build compilation database if possible
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

option(CLI "Build with cli." ON)
option(SERVER "Build with server." ON)
option(GUI "Build against GUI." OFF)
option(GUI_SANDBOX "Build against GUI sandbox (MAC Only)." OFF)
option(BUILD_TESTS "Build with test." OFF)
option(BUILD_BENCHMARKS "Build with benchmark." OFF)
option(OPTIMIZED_PROTOC "Force protobuf compiler to be built with optimization" OFF)
option(USE_TCMALLOC "Build with tcmalloc (linux only)" OFF)
option(USE_LIBCXX "Build with libc++" ON)
option(USE_NGHTTP2 "Build with libnghttp2" ON)
option(USE_MOLD "Build with mold linker" OFF)
option(USE_LLD "Build with lld linker" OFF)
option(USE_GOLD "Build with gold linker" OFF)
option(USE_ICF "Build with ICF" OFF)
option(USE_LTO "Build with LTO" OFF)
option(USE_CURL "Build with libcurl (test only)" ON)
option(USE_CARES "Build with c-ares" ON)
option(BUILD_SHARED_LIBS "Build with shared libraries." OFF)
option(USE_BUILTIN_CA_BUNDLE_CRT "Build with embedded ca-bundle.crt support" ON)
option(USE_MBEDTLS "Build with mbedtls support" ON)
option(USE_BALSA_HTTP_PARSER "Build with balsa http parser" ON)
option(USE_ICU "Build with icu" OFF)
option(USE_LEVELDB "Build with leveldb" OFF)
option(USE_SQLITE "Build with sqlite" OFF)

option(ENABLE_LTO "Enable build with LTO" ON)
option(ENABLE_LLD "Enable build with LLD" ON)
option(ENABLE_GOLD "Enable build with GOLD" ON)

if (ENABLE_LLD AND (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR LINKER_LLD) AND NOT COMPILER_APPLE_CLANG)
  # skipping apple clang
  set(USE_LLD ON)
  set(USE_GOLD OFF)
elseif (ENABLE_GOLD AND COMPILER_GCC AND NOT WIN32)
  set(USE_LLD OFF)
  set(USE_GOLD ON)
endif()

if (USE_MOLD OR USE_LLD OR USE_GOLD)
  set(USE_ICF ON)
else()
  set(ENABLE_LTO OFF)
endif()

if (ENABLE_CLANG_TIDY)
  set(ENABLE_LTO OFF)
endif()

# -fsanitize-blacklist doesn't work with lto
if (ASAN OR TSAN OR UBSAN OR MSAN)
  set(ENABLE_LTO OFF)
endif()

if (USE_MOLD)
  set(USE_LLD OFF)
  set(USE_GOLD OFF)
endif()

if (CMAKE_BUILD_TYPE)
  string(TOUPPER ${CMAKE_BUILD_TYPE} upper_CMAKE_BUILD_TYPE)
else()
  string(TOUPPER "Debug" upper_CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

string(TOLOWER ${CMAKE_BUILD_TYPE} lower_CMAKE_BUILD_TYPE)
list(APPEND YASS_APP_FEATURES "${lower_CMAKE_BUILD_TYPE}")

if (COMPILER_MSVC)
  list(APPEND YASS_APP_FEATURES "msvc ${CMAKE_CXX_COMPILER_VERSION}")
elseif(COMPILER_GCC)
  list(APPEND YASS_APP_FEATURES "gcc ${CMAKE_CXX_COMPILER_VERSION}")
elseif(COMPILER_APPLE_CLANG)
  list(APPEND YASS_APP_FEATURES "appleclang ${CMAKE_CXX_COMPILER_VERSION}")
elseif(COMPILER_CLANG)
  list(APPEND YASS_APP_FEATURES "clang ${CMAKE_CXX_COMPILER_VERSION}")
endif()

if (CMAKE_CROSSCOMPILING)
  list(APPEND YASS_APP_FEATURES "crosscompile")
endif()

if (${CMAKE_BUILD_TYPE} MATCHES Release OR ${CMAKE_BUILD_TYPE} MATCHES MinSizeRel OR ${CMAKE_BUILD_TYPE} MATCHES RelWithDebInfo)
  message(STATUS "Release Build: ${CMAKE_BUILD_TYPE}")
  set(IS_RELEASE_BUILD ON)
else()
  message(STATUS "Debug Build")
endif()

if (ENABLE_LTO AND IS_RELEASE_BUILD)
  if (COMPILER_CLANG)
    if (USE_MOLD AND UNIX AND NOT APPLE)
      set(LTO ON)
      set(LTO_FLAVOUR "thin")
    endif()

    if (USE_LLD)
      set(LTO ON)
      set(LTO_FLAVOUR "thin")
    endif()

    if (USE_GOLD)
      set(LTO ON)
      set(LTO_FLAVOUR "thin")
    endif()
  endif()

  ## adopt Full LTO
  ## old gcc has regression on c++17 mode
  ## https://gcc.gnu.org/bugzilla/show_bug.cgi?id=81004
  if (COMPILER_GCC AND NOT WIN32 AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.3.1)
    set(LTO ON)
    set(LTO_FLAVOUR "full")
  endif()

  if (LTO)
    message(STATUS "Compiling with ${LTO_FLAVOUR} LTO")
    list(APPEND YASS_APP_FEATURES "lto ${LTO_FLAVOUR}")
  endif()
endif()

if (COMPILER_MSVC AND USE_LIBCXX)
  message(SEND_ERROR "Microsoft Visual C++ Compiler with libc++ support is not supported, please disabling it with -DUSE_LIBCXX=off")
endif()

include(config-ix)

# see "Announcement: ARC is default for Objective-C code"
# https://groups.google.com/a/chromium.org/g/embedder-dev/c/13hNC6w2gGA?pli=1
if (APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-arc -fno-objc-arc-exceptions")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc -fno-objc-arc-exceptions")
endif()

# set default _MSC_VER
if (MSVC AND COMPILER_CLANG)
  if (ALLOW_XP)
    # look back to vs2017, fix issue of linkage of ___dyn_tls_on_demand_init and ___tls_guard defined in msvcrt/libcmt
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fmsc-version=1916")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmsc-version=1916")
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fmsc-version=1934")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmsc-version=1934")
  endif()
endif()

if (CMAKE_OSX_ARCHITECTURES)
  if (NOT "${CMAKE_OSX_ARCHITECTURES}" STREQUAL ${CMAKE_SYSTEM_PROCESSOR})
    set(USE_HOST_TOOLS ON)
  endif()
endif()

if (CMAKE_CROSSCOMPILING OR (OPTIMIZED_PROTOC AND CMAKE_CONFIGURATION_TYPES))
  set(USE_HOST_TOOLS ON)
endif()

if (CMAKE_CROSSCOMPILING AND CMAKE_SYSROOT)
  if (NOT EXISTS ${CMAKE_SYSROOT})
    message(FATAL_ERROR "Invalid sysroot ${CMAKE_SYSROOT}")
  endif()
  set(ENV{PKG_CONFIG_DIR} "")
  set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
  if (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/libdata/pkgconfig:${CMAKE_SYSROOT}/usr/local/libdata/pkgconfig")
  endif()
  set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
endif()

# cross-compiling mingw and freebsd don't like --gcc-install-dir (non-gnu style)
if (UNIX AND CMAKE_CROSSCOMPILING AND CMAKE_SYSROOT AND COMPILER_CLANG AND NOT CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.0)
    file(GLOB _GCC_SYSROOT "${CMAKE_SYSROOT}/lib/gcc/*/*.*.*")
    if (_GCC_SYSROOT)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --gcc-install-dir=${_GCC_SYSROOT}")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --gcc-install-dir=${_GCC_SYSROOT}")
    endif()
  else()
    # not useful usage:
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --gcc-toolchain=${CMAKE_SYSROOT}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --gcc-toolchain=${CMAKE_SYSROOT}")
  endif()
endif()

include(CrossCompile)
# when crosscompiling import the executable targets from a file
if (USE_HOST_TOOLS)
  if (CMAKE_OSX_ARCHITECTURES)
    set(OSX_NATIVE_ARCH_OPTION "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  create_cross_target(yass NATIVE "" Release ${OSX_NATIVE_ARCH_OPTION})
endif()

if (CMAKE_OSX_ARCHITECTURES)
  list(LENGTH CMAKE_OSX_ARCHITECTURES OSX_CROSS_ARCHITECTURES_COUNT)
  if(NOT OSX_CROSS_ARCHITECTURES_COUNT EQUAL 1)
    if (COMPILER_GCC)
      message(FATAL_ERROR "GCC is not supported to compiling universal build")
    endif()
    set(OSX_UNIVERSALBUILD ON)
    foreach (OSX_ARCH ${CMAKE_OSX_ARCHITECTURES})
      create_cross_target(yass OSX_${OSX_ARCH} "" ${CMAKE_BUILD_TYPE} -DCMAKE_OSX_ARCHITECTURES=${OSX_ARCH})
      set(OSX_CROSS_ARCHITECTURES ${OSX_CROSS_ARCHITECTURES} ${OSX_ARCH})
    endforeach()

    include(UniversalBuild)
    if (CLI)
      add_osx_univeral_target(yass_cli "${OSX_CROSS_ARCHITECTURES}" yass)
    endif()
    if (SERVER)
      add_osx_univeral_target(yass_server "${OSX_CROSS_ARCHITECTURES}" yass)
    endif()
    if (GUI)
      # this a dirty check for GUI flavor
      if (COMPILER_GCC)
        add_osx_univeral_target(yass "${OSX_CROSS_ARCHITECTURES}" yass)
      else()
        add_osx_univeral_bundle(yass "${OSX_CROSS_ARCHITECTURES}" yass)
      endif()
    endif()
    if (BUILD_TESTS)
      add_osx_univeral_target(yass_test "${OSX_CROSS_ARCHITECTURES}" yass)
      add_custom_target(check
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/universal/yass_test
        DEPENDS yass_test
        COMMENT "yass unittests"
        USES_TERMINAL
        )
    endif()
    if (BUILD_BENCHMARKS)
      add_osx_univeral_target(yass_benchmark "${OSX_CROSS_ARCHITECTURES}" yass)
    endif()

    # overrided default target with universal ones
    # turning them off
    set(CLI OFF)
    set(SERVER OFF)
    set(GUI OFF)
    set(BUILD_TESTS OFF)
    set(BUILD_BENCHMARKS OFF)
    set(CMAKE_OSX_ARCHITECTURES ${CMAKE_SYSTEM_PROCESSOR})
  else()
    set(OSX_UNIVERSALBUILD OFF)
  endif()
endif()

if(YASS_TARGET_IS_CROSSCOMPILE_HOST)
# Dummy use to avoid CMake Warning: Manually-specified variables were not used
# (this is a variable that CrossCompile sets on recursive invocations)
endif()

if (NOT CMAKE_SKIP_INSTALL_RULES)
  include(GNUInstallDirs)
endif()

# *****************************************************************************************
#           Debug facilities
# *****************************************************************************************

if(MSAN)
  if(ASAN)
    message(FATAL_ERROR "ASAN and MSAN are mutually exclusive")
  endif()

  if(NOT COMPILER_CLANG)
    # experimental
    # due to the ms document https://docs.microsoft.com/en-us/cpp/sanitizers/asan?view=msvc-160
    if (MSVC AND NOT MSVC_C_ARCHITECTURE_ID STREQUAL "arm64")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=memory")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=memory")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /fsanitize=memory")
    else()
      message(FATAL_ERROR "Cannot enable MSAN unless using Clang or Vistual Studio 2019 version 16.9")
    endif()
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
  endif()
  add_definitions(-DMEMORY_SANITIZER)
  list(APPEND YASS_APP_FEATURES "msan")
endif()

if(ASAN)
  if(NOT COMPILER_CLANG)
    # Current support is limited to x86 and x64 on Windows 10
    # due to the ms document https://docs.microsoft.com/en-us/cpp/sanitizers/asan?view=msvc-160
    if (MSVC AND NOT MSVC_C_ARCHITECTURE_ID STREQUAL "arm64")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /fsanitize=address")
    else()
      message(FATAL_ERROR "Cannot enable ASAN unless using Clang or Vistual Studio 2019 version 16.9")
    endif()
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
  endif()
  add_definitions(-DADDRESS_SANITIZER)
  list(APPEND YASS_APP_FEATURES "asan")
endif()

if(CFI)
  if(NOT COMPILER_CLANG)
    message(FATAL_ERROR "Cannot enable CFI unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=cfi -fno-sanitize-trap=cfi -flto=thin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=cfi -fno-sanitize-trap=cfi -flto=thin")
  # We use Chromium's copy of clang, which requires -fuse-ld=lld if building
  # with -flto. That, in turn, can't handle -ggdb.
  string(REPLACE "-ggdb" "-g" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REPLACE "-ggdb" "-g" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  list(APPEND YASS_APP_FEATURES "cfi")
endif()

if(TSAN)
  if(NOT COMPILER_CLANG)
    # experimental
    # due to the ms document https://docs.microsoft.com/en-us/cpp/sanitizers/asan?view=msvc-160
    if (MSVC AND NOT MSVC_C_ARCHITECTURE_ID STREQUAL "arm64")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=thread")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=thread")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /fsanitize=thread")
    else()
      message(FATAL_ERROR "Cannot enable TSAN unless using Clang or Vistual Studio 2019 version 16.9")
    endif()
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
  endif()
  add_definitions(-DTHREAD_SANITIZER)
  list(APPEND YASS_APP_FEATURES "tsan")
endif()

if(UBSAN)
  if(NOT COMPILER_CLANG)
    # experimental
    # due to the ms document https://docs.microsoft.com/en-us/cpp/sanitizers/asan?view=msvc-160
    if (MSVC AND NOT MSVC_C_ARCHITECTURE_ID STREQUAL "arm64")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=undefined")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=undefined")
    else()
      message(FATAL_ERROR "Cannot enable UBSAN unless using Clang or Vistual Studio 2019 version 16.9")
    endif()
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
  endif()
  add_definitions(-DUNDEFINED_SANITIZER)
  list(APPEND YASS_APP_FEATURES "ubsan")
endif()

# apply sanitizer blacklist
if (MSAN OR ASAN OR TSAN OR UBSAN)
  if (COMPILER_CLANG)
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -fsanitize-blacklist=${CMAKE_CURRENT_SOURCE_DIR}/src/blacklist.txt")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-blacklist=${CMAKE_CURRENT_SOURCE_DIR}/src/blacklist.txt")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize-blacklist=${CMAKE_CURRENT_SOURCE_DIR}/src/blacklist.txt")
  endif()
endif()

if(GCOV)
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  endif()
  list(APPEND YASS_APP_FEATURES "coverage")
endif()

if (NOT MSAN AND NOT ASAN AND NOT TSAN AND NOT UBSAN)
  if (NOT APPLE AND NOT MSVC AND NOT MINGW)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,defs -Wl,--as-needed")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,defs -Wl,--as-needed")
  endif()
endif()

# *****************************************************************************************
#           Compiler compiler and link flags (base)
# *****************************************************************************************

if (NOT MSVC)
  # Necessary for clone().
  if (ANDROID OR OHOS)
    add_definitions(-D__GNU_SOURCE=1)
  endif()

  ## optimization flags
  set(CMAKE_C_FLAGS_MINSIZEREL   "${CMAKE_C_FLAGS_MINSIZEREL} -DNDEBUG")
  set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DNDEBUG")
  set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

  ## can gc eliminate any unused functions and data items
  if (NOT APPLE)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections")
  endif()

  ## standard C++ library
  if(APPLE AND NOT USE_LIBCXX AND COMPILER_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  endif()

  if (APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-dead_strip")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip")
  else()
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
  endif()

  if (APPLE AND NOT USE_MOLD)
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-no_data_in_code_info -Wl,-no_function_starts")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,-no_data_in_code_info -Wl,-no_function_starts")
  endif()

  if (APPLE AND COMPILER_CLANG)
    # If generating dSYMs, specify -fno-standalone-debug. This was
    # originally specified for https://crbug.com/479841 because dsymutil
    # could not handle a 4GB dSYM file. But dsymutil from Xcodes prior to
    # version 7 also produces debug data that is incompatible with Breakpad
    # dump_syms, so this is still required (https://crbug.com/622406).
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-standalone-debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-standalone-debug")
  endif()

  # gcc lto doesn't like coff/macho with debuginfo
  #
  # And old gcc doesn't works well with "-flto" and "-g" options until GCC 8.0
  # And it doesn't play well on other file formats other than ELF
  # Checked with https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Optimize-Options.html#Optimize-Options
  if (NOT (COMPILER_GCC AND LTO_FLAVOUR AND (APPLE OR WIN32 OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)))
    # clang defaults to DWARF2 on macOS unless mac_deployment_target is
    # at least 10.11.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gdwarf-4")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-4")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g2")

    # generate a complete set of debugging symbols for your code.
    # The linker uses this extra debugging information to dead strip the executable.
    if (APPLE)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gfull")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gfull")
    endif()

    # gcc generates dwarf-aranges by default on -g1 and -g2. On clang it has
    # to be manually enabled
    if (COMPILER_CLANG)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gdwarf-aranges")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-aranges")
    endif()
  endif()

  if (COMPILER_CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
    if (APPLE)
      # TODO(https://crbug.com/1050118): Investigate missing debug info on mac.
      # Make sure we don't use constructor homing on mac.
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Xclang -debug-info-kind=limited")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -debug-info-kind=limited")
    else()
      # Use constructor homing for debug info. This option reduces debug info
      # by emitting class type info only when constructors are emitted
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Xclang -fuse-ctor-homing")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fuse-ctor-homing")
    endif()
  endif()

  # -ffile-compilation-dir is an alias for both -fdebug-compilation-dir=
  # and -fcoverage-compilation-dir=.
  if (COMPILER_CLANG AND NOT COMPILER_APPLE_CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14.0)
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,-fdebug-compilation-dir,.")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Xclang -fdebug-compilation-dir -Xclang . -ffile-compilation-dir=.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fdebug-compilation-dir -Xclang . -ffile-compilation-dir=.")
  endif()

  # Tells the compiler not to use absolute paths when passing the default
  # paths to the tools it invokes. We don't want this because we don't
  # really need it and it can mess up the goma cache entries.
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -no-canonical-prefixes")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-canonical-prefixes")

    # Same for links: Let the compiler driver invoke the linker
    # with a relative path and pass relative paths to built-in
    # libraries. Not needed on Windows because we call the linker
    # directly there, not through the compiler driver.
    # We don't link on goma, so this change is just for cleaner
    # internal linker invocations, for people who work on the build.
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -no-canonical-prefixes")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-canonical-prefixes")
  endif()
else()
  # Enable function-level linking.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gy")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Gy")

  # Preserve previous PDB behavior.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /FS")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FS")

  # Some of our files are bigger than the regular limits.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /bigobj")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")

  ## Set Source and Excutable character sets to UTF-8
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Zc:twoPhase")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:twoPhase")
  endif()

  # Consistently use backslash as the path separator when expanding the
  # __FILE__ macro when targeting Windows regardless of the build
  # environment.
  if (COMPILER_CLANG AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffile-reproducible")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffile-reproducible")
  endif()

  # Work around crbug.com/526851, bug in VS 2015 RTM compiler.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Zc:sizedDealloc-")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:sizedDealloc-")

  # Do not export inline member functions. This makes component builds
  # faster. This is similar to -fvisibility-inlines-hidden.
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zc:dllexportInlines-")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zc:dllexportInlines-")
  endif()

  ## Optimization flags
  set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Zc:inline /DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zc:inline /DNDEBUG")

  ## Disable LTCG for static libraries because this setting introduces ABI incompatibility between minor compiler versions
  if (NOT MSVC_CRT_LINKAGE STREQUAL "static" AND NOT COMPILER_CLANG)
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /GL")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
  endif()

  # lld's uac manifest is outdated and incompatible with windows xp
  if (USE_LLD)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFESTUAC:NO")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFESTUAC:NO")
  endif()

  ## not omit stack frame pointer
  set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Oy-")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oy-")

  ## supress some warnings
  if (COMPILER_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-microsoft-exception-spec")
  endif()

  ## Generate debug info, Debug information in the .obj files for clang.
  ## Produce PDB file for msvc, no edit and continue.
  if (COMPILER_CLANG AND NOT ENABLE_CLANG_TIDY)
    # Debug information in the .obj files.
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} /Z7")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Z7")
    # Disable putting the compiler command line into the debug info to
    # prevent some types of non-determinism.
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -gno-codeview-command-line")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gno-codeview-command-line")
    ## To generate PDB files for debugging with MSVC, you can use the /Z7 option.
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
      string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
      string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
      string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
      string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
      string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
      string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
      string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}")
    endif()

    # Turn this on to use ghash feature of lld for faster debug link on Windows.
    # http://blog.llvm.org/2018/01/improving-link-time-on-windows-with.html
    if (USE_LLD)
      set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -gcodeview-ghash")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gcodeview-ghash")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG:GHASH")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG:GHASH")
    endif()
  else()
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
  endif()

  # Tell linker to include symbol data
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")

  ## Remove unreferenced data
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /INCREMENTAL:NO /FIXED:NO")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /INCREMENTAL:NO /FIXED:NO")

  list(APPEND YASS_APP_FEATURES "ldflags icf")

  # Tell linker to include FIXUP information symbol data
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /PROFILE")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /PROFILE")

  # Use constructor homing for debug info. This option reduces debug info
  # by emitting class type info only when constructors are emitted
  if (COMPILER_CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Xclang -fuse-ctor-homing")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fuse-ctor-homing")
  endif()

  if (USE_LLD)
    # Use a fake fixed base directory for paths in the pdb to make the pdb
    # output fully deterministic and independent of the build directory.
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /PDBSourcePath:o:\\fake\\prefix")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /PDBSourcePath:o:\\fake\\prefix")
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} /PDBSourcePath:o:\\fake\\prefix")
    set(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "${CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL} /PDBSourcePath:o:\\fake\\prefix")

    # Absolutize source file paths for PDB. Pass the real build directory
    # if the pdb contains source-level debug information and if linker
    # reproducibility is not critical.
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /PDBSourcePath:${CMAKE_CURRENT_BINARY_DIR}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /PDBSourcePath:${CMAKE_CURRENT_BINARY_DIR}")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO   "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /PDBSourcePath:${CMAKE_CURRENT_BINARY_DIR}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /PDBSourcePath:${CMAKE_CURRENT_BINARY_DIR}")
  endif()

  # Tells the compiler not to use absolute paths when passing the default
  # paths to the tools it invokes. We don't want this because we don't
  # really need it and it can mess up the goma cache entries.
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -no-canonical-prefixes")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-canonical-prefixes")
  endif()
endif()

## adopt Thin LTO
if (LTO AND COMPILER_CLANG AND USE_MOLD)
  set(CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}   "${CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")
  set(CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")

  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
    # TODO(lgrey): Enable unit splitting for Mac when supported.
    set(CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}   "${CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fsplit-lto-unit")
    set(CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fsplit-lto-unit")
  endif()

  if (NOT MSVC)
    set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")
    set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")
    # In ThinLTO builds, we run at most one link process at a time,
    # and let it use all cores.
    # TODO(thakis): Check if '=0' (that is, number of cores, instead
    # of "all" which means number of hardware threads) is faster.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11.0)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-jobs=all")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-jobs=all")
    else()
      include(ProcessorCount)
      ProcessorCount(CPU_NUMBER)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-jobs=${CPU_NUMBER}")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-jobs=${CPU_NUMBER}")
    endif()
  endif()

  if (NOT ANDROID AND NOT OHOS AND (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR APPLE))
    set(CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
    set(CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
    if (NOT MSVC)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
    endif()
  endif()
endif()

## adopt Thin LTO
if (LTO AND COMPILER_CLANG AND USE_LLD)
  set(CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}   "${CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")
  set(CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")

  if (NOT APPLE AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
    # TODO(lgrey): Enable unit splitting for Mac when supported.
    set(CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}   "${CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fsplit-lto-unit")
    set(CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fsplit-lto-unit")
  endif()

  # Limit the size of the ThinLTO cache to the lesser of 10% of
  # available disk space, 40GB and 100000 files.
  if (MSVC)
    set(cache_policy "cache_size=10%:cache_size_bytes=40g")
  else()
    set(cache_policy "cache_size=10\\%:cache_size_bytes=40g")
  endif()

  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
    set(cache_policy "${cache_policy}:cache_size_files=100000")
  endif()

  # TODO(gbiv): We ideally shouldn't need to specify this; ThinLTO
  # should be able to better manage binary size increases on its own.
  set(import_instr_limit 5)

  if (MSVC)
    # When using lld-link, the -flto option need only be added to the compile step
    set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} /opt:lldltojobs=all -mllvm:-import-instr-limit=${import_instr_limit} /lldltocache:thinlto-cache /lldltocachepolicy:${cache_policy}")
    set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} /opt:lldltojobs=all -mllvm:-import-instr-limit=${import_instr_limit} /lldltocache:thinlto-cache /lldltocachepolicy:${cache_policy}")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 17.0)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -mllvm:-disable-auto-upgrade-debug-info")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -mllvm:-disable-auto-upgrade-debug-info")
    endif()
  elseif (NOT MINGW)
    set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")
    set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")
    # In ThinLTO builds, we run at most one link process at a time,
    # and let it use all cores.
    # TODO(thakis): Check if '=0' (that is, number of cores, instead
    # of "all" which means number of hardware threads) is faster.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11.0)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-jobs=all")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-jobs=all")
    else()
      include(ProcessorCount)
      ProcessorCount(CPU_NUMBER)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-jobs=${CPU_NUMBER}")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-jobs=${CPU_NUMBER}")
    endif()
    if (APPLE AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.0)
      if (YASS_TARGET_IS_CROSSCOMPILE_HOST)
        set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-cache_path_lto,${CMAKE_CURRENT_BINARY_DIR}/../thinlto-cache")
        set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-cache_path_lto,${CMAKE_CURRENT_BINARY_DIR}/../thinlto-cache")
      else()
        set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-cache_path_lto,thinlto-cache")
        set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-cache_path_lto,thinlto-cache")
      endif()
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wcrl,object_path_lto")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wcrl,object_path_lto")
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-cache-policy=${cache_policy}")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-cache-policy=${cache_policy}")
    elseif (NOT APPLE)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-cache-dir=thinlto-cache")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-cache-dir=thinlto-cache")
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-cache-policy,${cache_policy}")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,--thinlto-cache-policy,${cache_policy}")
    endif()

    set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-mllvm,-import-instr-limit=${import_instr_limit}")
    set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-mllvm,-import-instr-limit=${import_instr_limit}")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 17.0)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-mllvm,-disable-auto-upgrade-debug-info")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-mllvm,-disable-auto-upgrade-debug-info")
    endif()
  endif()

  if (NOT ANDROID AND NOT OHOS AND (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR APPLE))
    set(CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
    set(CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
    if (NOT MSVC)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
    endif()
  endif()
endif()

## adopt Thin LTO
if (LTO AND COMPILER_CLANG AND USE_GOLD)
  set(CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}   "${CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")
  set(CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")

  # Limit the size of the ThinLTO cache to the lesser of 10% of
  # available disk space, 40GB and 100000 files.
  set(cache_policy "cache_size=10\\%:cache_size_bytes=40g:cache_size_files=100000")

  # TODO(gbiv): We ideally shouldn't need to specify this; ThinLTO
  # should be able to better manage binary size increases on its own.
  set(import_instr_limit 5)

  set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")
  set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=thin")
  # In ThinLTO builds, we run at most one link process at a time,
  # and let it use all cores.
  # TODO(thakis): Check if '=0' (that is, number of cores, instead
  # of "all" which means number of hardware threads) is faster.
  set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-plugin-opt,jobs=all")
  set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-plugin-opt,jobs=all")

  set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-plugin-opt,cache-dir=thinlto-cache")
  set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-plugin-opt,cache-dir=thinlto-cache")

  set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-plugin-opt,cache-policy=${cache_policy}")
  set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-plugin-opt,cache-policy=${cache_policy}")

  set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-mllvm,-import-instr-limit=${import_instr_limit}")
  set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wl,-mllvm,-import-instr-limit=${import_instr_limit}")

  if (NOT ANDROID AND NOT OHOS AND (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR APPLE))
    set(CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
    set(CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
    if (NOT MSVC)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -fwhole-program-vtables")
    endif()
  endif()
endif()

## adopt Full LTO
## old gcc has regression on c++17 mode
## https://gcc.gnu.org/bugzilla/show_bug.cgi?id=81004
if (LTO AND COMPILER_GCC)
  set(CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}   "${CMAKE_C_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=auto")
  set(CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_CXX_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=auto")
  set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=auto")
  set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -flto=auto")
  # gcc is known to fails at combining -std=c++14 and -std=c++17 objects (fixed in gcc 8.4)
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89358
  # and another https://gcc.gnu.org/bugzilla/show_bug.cgi?id=84044 (fixed in gcc 9.1)
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wno-error=odr")
    set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wno-error=odr")
  endif()
endif()

# skipping for coff binaries on ar fix
if (NOT MSVC AND LTO)
  # althrough newer version of binutils' ar accepts lto objects, overriding it for compatiblity
  set(CMAKE_AR "${CMAKE_CXX_COMPILER_AR}")
  set(CMAKE_RANLIB "${CMAKE_CXX_COMPILER_RANLIB}")
endif()

# *****************************************************************************************
#           Platform-specific
# *****************************************************************************************

# Here we enable -fno-delete-null-pointer-checks, which makes various nullptr
# operations (e.g. dereferencing) into defined behavior. This avoids deletion
# of some security-critical code: see https://crbug.com/1139129.
# Nacl does not support the flag. And, we still want UBSAN to catch undefined
# behavior related to nullptrs, so do not add this flag if UBSAN is enabled.
# GCC seems to have some bugs compiling constexpr code when this is defined,
# so only enable it if using_clang. See: https://gcc.gnu.org/PR97913
# TODO(mpdenton): remove is_clang once GCC bug is fixed.
if (NOT UBSAN AND COMPILER_CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.0)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-delete-null-pointer-checks")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-delete-null-pointer-checks")
endif()

# Don't emit the GCC version ident directives, they just end up in the
# .comment section or debug info taking up binary size, and makes comparing
# .o files built with different compiler versions harder.
if (NOT MSVC AND COMPILER_CLANG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-ident")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-ident")
endif()

# In general, Windows is totally different, but all the other builds share
# some common compiler and linker configuration.
if (NOT MSVC)
  # Common POSIX compiler flags setup.
  # --------------------------------
  # See http://crbug.com/32204
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")

  # Stack protection.
  if (APPLE AND COMPILER_CLANG)
    # The strong variant of the stack protector significantly increases
    # binary size, so only enable it in debug mode.
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fstack-protector-strong")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fstack-protector-strong")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fstack-protector")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fstack-protector")
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -fstack-protector")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -fstack-protector")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fstack-protector")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fstack-protector")
  elseif (UNIX AND NOT APPLE)
    # TODO(phajdan.jr): Use -fstack-protector-strong when our gcc supports it.
    # See also https://crbug.com/533294
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --param=ssp-buffer-size=4")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --param=ssp-buffer-size=4")

    # FIXME The ANDROID x86 toolchain currently has problems with stack-protector.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector")
  endif()

  if (USE_MOLD)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=mold")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")
    list(APPEND YASS_APP_FEATURES "linker mold")
  endif()

  if (USE_LLD)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    list(APPEND YASS_APP_FEATURES "linker lld")
  endif()

  # Linker warnings.
  if (FALSE)
    if (NOT APPLE)
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--fatal-warnings")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--fatal-warnings")
    else ()
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-fatal_warnings")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-fatal_warnings")
    endif()
  endif()
endif()

# Non-Mac Posix compiler flags setup.
# -----------------------------------
if (UNIX AND NOT APPLE)
  # Explicitly pass --build-id to ld. Compilers used to always pass this
  # implicitly but don't any more (in particular clang when built without
  # ENABLE_LINKER_BUILD_ID=ON).
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--build-id=sha1")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--build-id=sha1")
  set(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "${CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL} -Wl,--build-id=sha1")
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -Wl,--build-id=sha1")
  set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} -Wl,--build-id=sha1")
  set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} -Wl,--build-id=sha1")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -Wl,--build-id")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -Wl,--build-id")

  # _FILE_OFFSET_BITS=64 should not be set on Android in order to maintain
  # the behavior of the Android NDK from earlier versions.
  # See https://android-developers.googleblog.com/2017/09/introducing-android-native-development.html
  if (NOT ANDROID AND NOT OHOS)
    add_definitions(-D_FILE_OFFSET_BITS=64)
    add_definitions(-D_LARGEFILE_SOURCE)
    add_definitions(-D_LARGEFILE64_SOURCE)
  endif()

  # unwind-tables
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -funwind-tables")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funwind-tables")
endif()

# Linux/Android/Fuchsia common flags setup.
# ---------------------------------
if ((UNIX AND NOT APPLE) OR ANDROID OR OHOS)
  # Enable position-independent code globally. This is needed because
  # some library targets are OBJECT libraries.
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

  # Use pipes for communicating between sub-processes. Faster.
  # (This flag doesn't do anything with Clang.)
  if (NOT COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe")
  endif()

  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,noexecstack -Wl,-z,relro")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,noexecstack -Wl,-z,relro")

  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,now")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,now")
endif()

# Linux-specific compiler flags setup.
# ------------------------------------
if ((UNIX AND NOT APPLE) AND USE_LLD)
  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    # Reduce the page size from 65536 in order to reduce binary size slightly
    # by shrinking the alignment gap between segments. This also causes all
    # segments to be mapped adjacently, which breakpad relies on.
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=4096")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,max-page-size=4096")
  endif()
elseif (USE_GOLD)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
  list(APPEND YASS_APP_FEATURES "linker gold")
  if (NOT ANDROID AND NOT OHOS)
    # On Android, this isn't needed.  gcc in the NDK knows to look next to
    # it with -fuse-ld=gold, and clang gets a --gcc-toolchain flag passed
    # above.

    # Experimentation found that using four linking threads
    # saved ~20% of link time.
    # https://groups.google.com/a/chromium.org/group/chromium-dev/browse_thread/thread/281527606915bb36
    # Only apply this to the target linker, since the host
    # linker might not be gold, but isn't used much anyway.
    # FIXME crashing on aarch64 build machine
    if (FALSE)
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--threads -Wl,--thread-count=4")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--threads -Wl,--thread-count=4")
    endif()
  endif()

  # TODO(thestig): Make this flag work with GN.
  #if (NOT ASAN AND NOT TSAN AND NOT MSAN AND NOT LSAN) {
  #    "-Wl,--detect-odr-violations",
  #}
endif()

if (USE_ICF AND (NOT APPLE OR COMPILER_CLANG) AND NOT MSVC)
  # Gold doesn't respect section alignment and breaks gcc builds with icf
  # https://sourceware.org/bugzilla/show_bug.cgi?id=17704
  # Landed upstream Fri, 21 Oct 2016 - is in 2.28, such as CentOS 7
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--icf=all")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--icf=all")
  list(APPEND YASS_APP_FEATURES "ldflags icf")
endif()

if (UNIX AND NOT APPLE)
  # Do not use the -pthread ldflag here since it becomes a no-op
  # when using -nodefaultlibs, which would cause an unused argument
  # error.  "-lpthread" is added in //build/config:default_libs.
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -pthread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# similar workaround exists inside libc++ directory
if (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  link_libraries(pthread)
endif()

# Clang-specific compiler flags setup.
# ------------------------------------
if (COMPILER_CLANG)
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fcolor-diagnostics")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
  if (MSVC AND ${CMAKE_GENERATOR} MATCHES "Ninja")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fansi-escape-codes")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fansi-escape-codes")
  endif()
  # Enable -fmerge-all-constants. This used to be the default in clang
  # for over a decade. It makes clang non-conforming, but is fairly safe
  # in practice and saves some binary size. We might want to consider
  # disabling this (https://bugs.llvm.org/show_bug.cgi?id=18538#c13),
  # but for now it looks like our build might rely on it
  # (https://crbug.com/829795).
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fmerge-all-constants")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmerge-all-constants")
endif()

if (USE_LLD)
  # TODO(thakis): Make the driver pass --color-diagnostics to the linker
  # if -fcolor-diagnostics is passed to it, and pass -fcolor-diagnostics
  # in ldflags instead.
  if (MSVC)
    # On Windows, we call the linker directly, instead of calling it through
    # the driver.
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --color-diagnostics")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --color-diagnostics")
  elseif (NOT MINGW)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--color-diagnostics")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -Wl,--color-diagnostics")
  endif()
endif()

# Enable text section splitting only on linux when using lld for now. Other
# platforms can be added later if needed.
if ((UNIX AND NOT APPLE) AND USE_LLD)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,keep-text-section-prefix")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,keep-text-section-prefix")
endif()

if (COMPILER_CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
  # TODO(hans): Remove this once Clang generates better optimized debug info
  # by default. https://crbug.com/765793
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -mllvm -instcombine-lower-dbg-declare=0")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mllvm -instcombine-lower-dbg-declare=0")
  if (USE_LLD)
    if (MSVC)
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -mllvm:-instcombine-lower-dbg-declare=0")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mllvm:-instcombine-lower-dbg-declare=0")
    else()
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-mllvm,-instcombine-lower-dbg-declare=0")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-mllvm,-instcombine-lower-dbg-declare=0")
    endif()
  endif()

  # TODO(crbug.com/1235145): Investigate why/if this should be needed.
  if (MSVC)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} /clang:-ffp-contract=off")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /clang:-ffp-contract=off")
  else()
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -ffp-contract=off")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffp-contract=off")
  endif()
endif()

# GCC-specific compiler flags setup.
# ------------------------------------
if (COMPILER_GCC)
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()

# C11/C++17 compiler flags setup.
# --------------------------
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (${CMAKE_GENERATOR} MATCHES "^Xcode.*")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
endif()

# https://chromium.googlesource.com/chromium/src/build/config/+/refs/heads/main/compiler/BUILD.gn
if (UNIX AND NOT APPLE)
  # Since we build with -std=c* and not -std=gnu*, _GNU_SOURCE will not be
  # defined by the compiler.  However, lots of code relies on the
  # non-standard features that _GNU_SOURCE enables, so define it manually.
  if (COMPILER_CLANG)
    add_definitions(-D_GNU_SOURCE)
  endif()
endif()

if (COMPILER_CLANG)
  # C++17 removes trigraph support, but clang still warns that it ignores
  # them when seeing them.  Don't.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-trigraphs")
endif()

# Pass flag to LLD so Android builds can allow debuggerd to properly symbolize
# stack crashes (http://crbug.com/919499).
if (USE_LLD AND ANDROID)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-rosegment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-rosegment")
endif()

# TODO(crbug.com/1374347): Cleanup undefined symbol errors caught by
# --no-undefined-version.
if (USE_LLD AND NOT WIN32 AND NOT APPLE AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.0)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--undefined-version")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--undefined-version")
endif()

# Apple: pass --strict-auto-link to ld64.lld
#
# Previously ld64.lld would always warn if LC_LINKER_OPTIONS specified
# a nonexistent library.
#
# After https://reviews.llvm.org/D140491, this will only happen if the link fails,
# matching ld64. Passing `--strict-auto-link` restores the old behavior.
if (USE_LLD AND APPLE AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.0)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--strict-auto-link")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--strict-auto-link")
endif()

# LLD does call-graph-sorted binary layout by default when profile data is
# present. On Android this increases binary size due to more thinks for long
# jumps. Turn it off by default and enable selectively for targets where it's
# beneficial.
if (USE_LLD)
  if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /call-graph-profile-sort:no")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /call-graph-profile-sort:no")
  elseif (NOT APPLE AND NOT MINGW AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
    # TODO(thakis): Once LLD's Mach-O port basically works, implement call
    # graph profile sorting for it, add an opt-out flag, and pass it here.
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-call-graph-profile-sort")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-call-graph-profile-sort")
  endif()
endif()

if (COMPILER_CLANG AND SHOW_INCLUDES)
  if (MSVC)
    # TODO(crbug.com/1223741): Goma mixes the -H and /showIncludes output.
    # assert(!use_goma, "show_includes on Windows is not reliable with goma")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /clang:-H /clang:-fshow-skipped-includes")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /clang:-H /clang:-fshow-skipped-includes")
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -H -fshow-skipped-includes")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -H -fshow-skipped-includes")
  endif()
endif()

# This flag enforces that member pointer base types are complete. It helps
# prevent us from running into problems in the Microsoft C++ ABI (see
# https://crbug.com/847724).
if (COMPILER_CLANG AND (MSVC OR USE_LIBCXX) AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.0)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcomplete-member-pointers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcomplete-member-pointers")
endif()

# Use DWARF simple template names, with the following exceptions:
#
# * Windows is not supported as it doesn't use DWARF.
# * Apple platforms (e.g. MacOS, iPhone, iPad) aren't supported because xcode
#   lldb doesn't have the needed changes yet.
# TODO(crbug.com/1379070): Remove if the upstream default ever changes.
#  Using simple template names results in considerable decrease in size in debug builds.
if (COMPILER_CLANG AND NOT MSVC AND NOT APPLE AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14.0)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gsimple-template-names")
endif()

if (WIN32)
  option(ALLOW_XP "Build with Windows XP support." OFF)
  ## //
  ## // _WIN32_WINNT version constants
  ## //
  ## #define _WIN32_WINNT_NT4                    0x0400 // Windows NT 4.0
  ## #define _WIN32_WINNT_WIN2K                  0x0500 // Windows 2000
  ## #define _WIN32_WINNT_WINXP                  0x0501 // Windows XP
  ## #define _WIN32_WINNT_WS03                   0x0502 // Windows Server 2003
  ## #define _WIN32_WINNT_WIN6                   0x0600 // Windows Vista
  ## #define _WIN32_WINNT_VISTA                  0x0600 // Windows Vista
  ## #define _WIN32_WINNT_WS08                   0x0600 // Windows Server 2008
  ## #define _WIN32_WINNT_LONGHORN               0x0600 // Windows Vista
  ## #define _WIN32_WINNT_WIN7                   0x0601 // Windows 7
  ## #define _WIN32_WINNT_WIN8                   0x0602 // Windows 8
  ## #define _WIN32_WINNT_WINBLUE                0x0603 // Windows 8.1
  ## #define _WIN32_WINNT_WINTHRESHOLD           0x0A00 // Windows 10
  ## #define _WIN32_WINNT_WIN10                  0x0A00 // Windows 10

  ## definitions FOR CRT
  add_definitions(-D__STD_C)
  add_definitions(-D_CRT_RAND_S)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
  add_definitions(-D_SCL_SECURE_NO_DEPRECATE)

  ## definitions FOR Windows SDK
  add_definitions(-D_WIN32)
  add_definitions(-DWIN32)
  add_definitions(-D_WINDOWS)
  add_definitions(-D_ATL_NO_OPENGL)
  add_definitions(-DNOMINMAX)
  add_definitions(-DUNICODE)
  add_definitions(-DWIN32_LEAN_AND_MEAN)
  add_definitions(-D_UNICODE)
  add_definitions(-DXP_OS2)
  add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)

  # not targeting Windows Runtime
  add_definitions(-DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP)

  if (ALLOW_XP)
    message(STATUS "Build with Windows XP support")
    add_definitions(-D_USING_V110_SDK71_)
    add_definitions(-D_WIN32_WINNT=0x0501)
    add_definitions(-DWINVER=0x0501)
    # for winapifamily.h
    if (NOT MINGW)
      include_directories(third_party/xp-compatible SYSTEM)
    endif()
    list(APPEND YASS_APP_FEATURES "winxp")
  elseif(OS_AARCH64)
    message(STATUS "Build with Windows 10 support")
    add_definitions(-D_WIN32_WINNT=0x0A00)
    add_definitions(-DWINVER=0x0A00)
    list(APPEND YASS_APP_FEATURES "win10")
  else()
    message(STATUS "Build with Windows 8.1 support")
    add_definitions(-D_WIN32_WINNT=0x0603)
    add_definitions(-DWINVER=0x0603)
    list(APPEND YASS_APP_FEATURES "win8.1")
  endif()

  # add missing definitions in some sdks
  # from sdkddkver.h
  add_definitions(-D_WIN32_WINNT_WINXP=0x0501)
  add_definitions(-D_WIN32_WINNT_WS03=0x0502)
  add_definitions(-D_WIN32_WINNT_VISTA=0x0600)
  add_definitions(-D_WIN32_WINNT_WIN6=0x0600)
  add_definitions(-D_WIN32_WINNT_LONGHORN=0x0600)
  add_definitions(-D_WIN32_WINNT_WIN7=0x0601)
  add_definitions(-D_WIN32_WINNT_WIN8=0x0602)
  add_definitions(-D_WIN32_WINNT_WINBLUE=0x0603)
  add_definitions(-D_WIN32_WINNT_WINTHRESHOLD=0x0A00)
  add_definitions(-D_WIN32_WINNT_WIN10=0x0A00)

  # set linker option for x86
  if (MSVC_PROCESSOR_ARCHITECTURE STREQUAL "x86")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH /largeaddressaware")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH /largeaddressaware")
  endif()

  if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DYNAMICBASE /NXCOMPAT")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DYNAMICBASE /NXCOMPAT")
  endif()

  if (MSVC AND ALLOW_XP)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Zc:threadSafeInit-")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:threadSafeInit-")
  endif()

  # set linker for subsystem
  # https://learn.microsoft.com/en-us/cpp/build/reference/subsystem-specify-subsystem?view=msvc-170
  if (MSVC OR (MINGW AND USE_LLD))
    if (ALLOW_XP)
      # 5.01 = Windows XP (minimum for x86 build)
      # 5.02 = Windows Server 2003/XP SP2 (minimum for x64 build)
      if (OS_X86)
        set(SUBSYSTEM_VERSION_SUFFIX ",5.01")
      else()
        set(SUBSYSTEM_VERSION_SUFFIX ",5.02")
      endif()
    elseif(OS_AARCH64)
      # Windows ARM64 requires Windows 10
      set(SUBSYSTEM_VERSION_SUFFIX ",10.0")
    else()
      # 6.02 = Windows 8.1
      set(SUBSYSTEM_VERSION_SUFFIX ",6.02")
    endif()
  endif()
  if (MSVC)
    set(CMAKE_C_CREATE_WIN32_EXE "/subsystem:windows${SUBSYSTEM_VERSION_SUFFIX}")
    set(CMAKE_CXX_CREATE_WIN32_EXE "/subsystem:windows${SUBSYSTEM_VERSION_SUFFIX}")
    set(CMAKE_C_CREATE_CONSOLE_EXE "/subsystem:console${SUBSYSTEM_VERSION_SUFFIX}")
    set(CMAKE_CXX_CREATE_CONSOLE_EXE "/subsystem:console${SUBSYSTEM_VERSION_SUFFIX}")
  else()
    set(CMAKE_C_CREATE_WIN32_EXE "-mwindows -Xlinker --subsystem -Xlinker windows${SUBSYSTEM_VERSION_SUFFIX}")
    set(CMAKE_CXX_CREATE_WIN32_EXE "-mwindows -Xlinker --subsystem -Xlinker windows${SUBSYSTEM_VERSION_SUFFIX}")
    set(CMAKE_C_CREATE_CONSOLE_EXE "-mconsole -Xlinker --subsystem -Xlinker console${SUBSYSTEM_VERSION_SUFFIX}")
    set(CMAKE_CXX_CREATE_CONSOLE_EXE "-mconsole -Xlinker --subsystem -Xlinker console${SUBSYSTEM_VERSION_SUFFIX}")
  endif()
endif()

# On windows preload settings
if (WIN32)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/WindowsCache.cmake)
endif()

# requires sse3, the next is avx
if (WIN32)
  # FIXME for mingw, CMAKE_SYSTEM_PROCESSOR can't tell which target it is building,
  # it needs to exclude arm64 build correctly.
  if ((MINGW AND NOT OS_AARCH64) OR MSVC_PROCESSOR_ARCHITECTURE STREQUAL "x86" OR MSVC_PROCESSOR_ARCHITECTURE STREQUAL "amd64")
    message(STATUS "Built with sse3 support")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -msse3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse3")
    list(APPEND YASS_APP_FEATURES "sse3")
  endif()
endif()

# comes from build/config/android/BUILD.gn
if (ANDROID)
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fno-short-enums")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-short-enums")

  add_definitions(-DANDROID)

  # The NDK has these things, but doesn't define the constants to say that it
  # does. Define them here instead.
  add_definitions(-DHAVE_SYS_UIO_H=1)

  # Forces full rebuilds on NDK rolls. To rebuild everything when NDK version
  # stays the same, increment the suffix number.
  add_definitions(-DANDROID_NDK_VERSION_ROLL=${ANDROID_API_VERSION})

  # Don't allow visible symbols from libraries that contain
  # assembly code with symbols that aren't hidden properly.
  # http://crbug.com/448386
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--exclude-libs=libvpx_assembly_arm.a")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--exclude-libs=libvpx_assembly_arm.a")

  # Reduce the page size from 65536 in order to reduce binary size slightly
  # by shrinking the alignment gap between segments. This also causes all
  # segments to be mapped adjacently, which breakpad relies on.
  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^arm64.*")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=4096")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,max-page-size=4096")
  endif()

  # Instead of using an unwind lib from the toolchain,
  # buildtools/third_party/libunwind will be built and used directly.
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --unwindlib=none")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --unwindlib=none")

  list(APPEND YASS_APP_FEATURES "libunwind")

  # for arm 32-bit v7, use correct march
  # following https://developer.android.com/ndk/guides/abis#v7a
  # thumb is enabled in v7 and neon is optional
  # following https://developer.android.com/ndk/guides/cpu-arm-neon
  # Almost all ARMv7-based Android devices support Neon, including all devices that
  # shipped with API level 21 or later. The NDK enables Neon by default.
  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^armeabi.*")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -march=armv7-a -mfloat-abi=softfp -mthumb -mfpu=neon")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfloat-abi=softfp -mthumb -mfpu=neon")
  endif()

  # for arm64-v8a
  # You can use Neon intrinsics in C and C++ code to take advantage of the Advanced SIMD extension.
  # following https://developer.android.com/ndk/guides/abis#arm64-v8a
  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^arm64.*")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -march=armv8-a+simd+fp")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+simd+fp")
  endif()

  # for x64 arch
  # following https://developer.android.com/ndk/guides/abis#x64
  # Android's ABI includes the base instruction set plus MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, and the POPCNT instruction.
  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^x64$")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -msse3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse3")
  endif()

  # for x86 arch
  # following https://developer.android.com/ndk/guides/abis#x86
  # Android's ABI includes the base instruction set plus the MMX, SSE, SSE2, SSE3, and SSSE3 extensions.
  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^x86$")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -mfpmath=sse -msse3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpmath=sse -msse3")
  endif()
endif()

# *****************************************************************************************
#           Libc++ Library
# *****************************************************************************************

if (USE_LIBCXX)
  add_definitions(-DHAVE_LIBCXX)
  add_subdirectory(third_party/libc++)
  link_libraries(cxx ${libcxx_PUBLIC_LIBRARIES})
  set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES} ${libcxx_PUBLIC_LIBRARIES}")
  list(APPEND YASS_APP_FEATURES "libc++")

  if (MINGW_MSVCRT100)
    list(APPEND YASS_APP_FEATURES "msvcrt100")
  endif()
  if (MINGW_WORKAROUND)
    list(APPEND YASS_APP_FEATURES "mingw atexit workaround")
  endif()
endif()

# *****************************************************************************************
#           Third Party Libraries
# *****************************************************************************************

include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(CheckLibraryExists)

check_c_source_compiles ("
#define STDOUT_FILENO 1
#include <fcntl.h>
int main() {
  static struct flock w_lock;

  w_lock.l_type = F_WRLCK;
  w_lock.l_start = 0;
  w_lock.l_whence = SEEK_SET;
  w_lock.l_len = 0;

  return fcntl(STDOUT_FILENO, F_SETLK, &w_lock);
}
" HAVE_FLOCK)
if (HAVE_FLOCK)
  add_definitions(-DHAVE_FLOCK)
endif()

if (NOT WIN32 AND NOT APPLE)
  check_library_exists(c pipe2 "" HAVE_PIPE2)
  check_library_exists(c dup3 "" HAVE_DUP3)
endif()
if (HAVE_PIPE2)
  add_definitions(-DHAVE_PIPE2)
endif()
if (HAVE_DUP3)
  add_definitions(-DHAVE_DUP3)
endif()

# *****************************************************************************************
#           Core Support Libraries
# *****************************************************************************************

if (WIN32)
  # ignore default set of libraries
  # aka KERNEL32 USER32 GDI32 WINSPOOL SHELL32 OLE32 OLEAUT32 UUID COMDLG32 ADVAPI32
  set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
  set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

  if (MSVC)
    set(SYSTEM_LIBS ${SYSTEM_LIBS} Crypt32 WS2_32 bcrypt User32 shell32 kernel32)
  else()
    set(SYSTEM_LIBS ${SYSTEM_LIBS} crypt32 ws2_32 mswsock bcrypt user32 shell32 kernel32)
  endif()

  # Don't use it even if we are compiling with gnu extensions
  if (MINGW)
    add_definitions(-D__USE_MINGW_ANSI_STDIO=0)
  endif()

  if (MSVC_CRT_LINKAGE STREQUAL "static")
    # replace /MD with /MT
    set(CompilerFlags
      CMAKE_CXX_FLAGS
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_MINSIZEREL
      CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_RELWITHDEBINFO
      CMAKE_C_FLAGS
      CMAKE_C_FLAGS_DEBUG
      CMAKE_C_FLAGS_MINSIZEREL
      CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_RELWITHDEBINFO
      )
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MDd" "/MTd" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
  endif()

  # disable precompiled head files for clang
  if (MSVC AND COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Y-")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Y-")
  endif()
else()
  if (CMAKE_FIND_ROOT_PATH)
    foreach(ROOT_PATH ${CMAKE_FIND_ROOT_PATH})
      include_directories(${ROOT_PATH}/include SYSTEM)
      link_directories(${ROOT_PATH}/lib)
    endforeach()
  endif()

# simple socketpair test code
set(SOCKETPAIR_C_CXX_TEST_SOURCE [====[
#include <sys/types.h>
#include <sys/socket.h>
int main() {
    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
}
]====])
  check_c_source_compiles("${SOCKETPAIR_C_CXX_TEST_SOURCE}"
      HAVE_SOCKETPAIR)
  if (HAVE_SOCKETPAIR)
      add_definitions(-DHAVE_SOCKETPAIR)
  endif()

  check_include_file(pwd.h HAVE_PWD_INCLUDE)
  if (HAVE_PWD_INCLUDE)
    add_definitions(-DHAVE_PWD_H)
  endif()

  # linux-syscall-support
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID OR OHOS)
    include_directories(third_party/lss)
  endif()
  if (APPLE AND COMPILER_CLANG)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -framework Foundation -framework SystemConfiguration")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Foundation -framework SystemConfiguration")
  endif()
endif()

link_libraries(${SYSTEM_LIBS})

# *****************************************************************************************
#           GUI Support Libraries
# *****************************************************************************************

if (ANDROID AND GUI)
  set(GUI_FLAVOUR "android")
  set(GUI_USE_FILE "")
  set(GUI_INCLUDE_DIRS "")
  set(GUI_DEFINITIONS "")
  set(GUI_C_CXX_FLAGS "")
  set(GUI_LIBRARY_DIRS "")
  set(GUI_LIBRARIES "")
elseif (WIN32 AND GUI)
  message(STATUS "Compiling with GUI support: Windows")
  set(GUI_FLAVOUR "windows")
  set(GUI_USE_FILE "")
  set(GUI_INCLUDE_DIRS "")
  set(GUI_DEFINITIONS "")
  set(GUI_C_CXX_FLAGS "")
  set(GUI_LIBRARY_DIRS "")
  if (MSVC)
    set(GUI_LIBRARIES RASAPI32 wininet ShLwApi WinMM Gdi32 ComDlg32 Ole32 ComCtl32 shell32)
  elseif (MINGW AND OS_AARCH64)
    # FIXME missing RasEnumEntriesW symbol from mingw
    set(GUI_LIBRARIES wininet shlwapi winmm gdi32 comdlg32 ole32 comctl32 shell32)
  else()
    set(GUI_LIBRARIES rasapi32 wininet shlwapi winmm gdi32 comdlg32 ole32 comctl32 shell32)
  endif()
elseif (APPLE AND GUI AND NOT IOS)
  message(STATUS "Compiling with GUI support: Cocoa")
  find_library(COCOA_LIBRARY Cocoa REQUIRED)
  # Locate Interface Builder Tool, needed to build things like Storyboards outside of Xcode.
  if(NOT ${CMAKE_GENERATOR} MATCHES "^Xcode.*")
    # Make sure we can find the 'ibtool' program. If we can NOT find it we skip generation of this project.
    find_program(IBTOOL ibtool HINTS "/usr/bin")
    if(${IBTOOL} STREQUAL "IBTOOL-NOTFOUND")
      message(WARNING "ibtool can not be found and is needed to compile the .xib files. "
                         "It should have been installed with the Apple developer tools. ")
      set(GUI off)
      message(WARNING "ibtool not found in path, disabling GUI build")
    endif()
  endif()

  set(GUI_FLAVOUR "cocoa")
  set(GUI_USE_FILE "")
  set(GUI_INCLUDE_DIRS "")
  set(GUI_DEFINITIONS "")
  set(GUI_C_CXX_FLAGS "")
  set(GUI_LIBRARY_DIRS "")
  set(GUI_LIBRARIES "${COCOA_LIBRARY}")
elseif (IOS AND GUI)
  message(STATUS "Compiling with GUI support: IOS")
  find_library(UIKIT_LIBRARY UIKit REQUIRED)
  # Locate Interface Builder Tool, needed to build things like Storyboards outside of Xcode.
  if(NOT ${CMAKE_GENERATOR} MATCHES "^Xcode.*")
    # Make sure we can find the 'ibtool' program. If we can NOT find it we skip generation of this project.
    find_program(IBTOOL ibtool HINTS "/usr/bin")
    if(${IBTOOL} STREQUAL "IBTOOL-NOTFOUND")
      message(WARNING "ibtool can not be found and is needed to compile the .xib files. "
                         "It should have been installed with the Apple developer tools. ")
      set(GUI off)
      message(WARNING "ibtool not found in path, disabling GUI build")
    endif()
  endif()

  set(GUI_FLAVOUR "ios")
  set(GUI_USE_FILE "")
  set(GUI_INCLUDE_DIRS "")
  set(GUI_DEFINITIONS "")
  set(GUI_C_CXX_FLAGS "")
  set(GUI_LIBRARY_DIRS "")
  set(GUI_LIBRARIES "${UIKIT_LIBRARY}")
elseif (GUI)
  message(STATUS "Compiling with GUI support: GTK+")
  find_package(PkgConfig)
  if (NOT PKG_CONFIG_FOUND)
    message(WARNING "pkg-config (required to search gtk+ path) not found, disabling GUI build")
    set(GUI off)
  else()
    pkg_check_modules(FONTCONFIG fontconfig)
    if (NOT FONTCONFIG_FOUND)
      message(WARNING "fontconfig not found in pkg-config, disabling GUI build")
      set(GUI off)
    endif()

    include(FindGettext)
    if (NOT GETTEXT_FOUND)
      message(WARNING "gettext not found, disabling GUI build")
      set(GUI off)
    endif()

    pkg_check_modules(GTK4 gtk4)
    if (GTK4_FOUND)
      set(GUI_FLAVOUR "gtk4")
      set(GUI_OTHER_FLAGS -Wno-deprecated-declarations)

      find_program(GLIB_COMPILE_RESOURCES glib-compile-resources HINTS "/usr/bin")
      if(${GLIB_COMPILE_RESOURCES} STREQUAL "GLIB_COMPILE_RESOURCES-NOTFOUND")
        message(WARNING "glib-compile-resources can not be found and is needed to compile the .gresource files. "
                        "It should have been installed with the glib2 library. ")
        set(GUI off)
      endif()

      if (GUI)
        set(GUI_USE_FILE "")
        set(GUI_INCLUDE_DIRS ${GTK4_INCLUDE_DIRS} ${FONTCONFIG_INCLUDE_DIRS})
        set(GUI_DEFINITIONS "")
        set(GUI_C_CXX_FLAGS ${GTK4_CFLAGS} ${GTK4_CFLAGS_OTHER}
          ${FONTCONFIG_CFLAGS} ${GUI_OTHER_FLAGS})
        set(GUI_LIBRARY_DIRS ${GTK4_LIBRARY_DIRS} ${FONTCONFIG_LIBRARY_DIRS})
        set(GUI_LIBRARIES ${GTK4_LIBRARIES} ${FONTCONFIG_LIBRARIES})
      endif()
    else()
      pkg_check_modules(GTK gtk+-3.0)
      set(GUI_FLAVOUR "gtk3")
      set(GUI_OTHER_FLAGS -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED)
      if (NOT GTK_FOUND)
        message(WARNING "Gtk+ not found in pkg-config, disabling GUI build")
        set(GUI off)
      endif()
      if (GUI)
        set(GUI_USE_FILE "")
        set(GUI_INCLUDE_DIRS ${GTK_INCLUDE_DIRS} ${FONTCONFIG_INCLUDE_DIRS})
        set(GUI_DEFINITIONS "")
        set(GUI_C_CXX_FLAGS ${GTK_CFLAGS} ${GTK_CFLAGS_OTHER}
          ${FONTCONFIG_CFLAGS} ${GUI_OTHER_FLAGS})
        set(GUI_LIBRARY_DIRS ${GTK_LIBRARY_DIRS} ${FONTCONFIG_LIBRARY_DIRS})
        set(GUI_LIBRARIES ${GTK_LIBRARIES} ${FONTCONFIG_LIBRARIES})
      endif()
    endif()
  endif()
endif()

# *****************************************************************************************
#           Compiler compiler and link flags (common)
# *****************************************************************************************

# symbol hiddens
if (NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -fvisibility=hidden")
endif()

if (NOT MSVC)
  ## definitions
  add_definitions(-D__STDC_CONSTANT_MACROS)
  add_definitions(-D__STDC_FORMAT_MACROS)

  ## disable rtti
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

  ## disable exceptions
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

  ## not omit stack frame pointer for release
  set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")

  ## fortify source code. Also, fortified build may fail when optimizations are
  ## disabled, so only do that for Release build.
  if (NOT WIN32 AND NOT APPLE)
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -D_FORTIFY_SOURCE=2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_FORTIFY_SOURCE=2")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} -D_FORTIFY_SOURCE=2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -D_FORTIFY_SOURCE=2")
  endif()

  ## set rpath origin
  if (NOT APPLE)
    # Setting this to true makes sure that libraries we build will have our
    # rpath set even without having to do "make install"
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
  endif()
else()
  ## Disable RTTI
  set(CompilerFlags
      CMAKE_CXX_FLAGS
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_MINSIZEREL
      CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_RELWITHDEBINFO
      CMAKE_C_FLAGS
      CMAKE_C_FLAGS_DEBUG
      CMAKE_C_FLAGS_MINSIZEREL
      CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_RELWITHDEBINFO
      )
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/GR" "/GR-" ${CompilerFlag} "${${CompilerFlag}}")
  endforeach()

  ## Disable RTTI
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

  # Disables exceptions in the STL.
  # libc++ uses the __has_feature macro to control whether to use exceptions,
  # so defining this macro is unnecessary. Defining _HAS_EXCEPTIONS to 0 also
  # breaks libc++ because it depends on MSVC headers that only provide certain
  # declarations if _HAS_EXCEPTIONS is 1. Those MSVC headers do not use
  # exceptions, despite being conditional on _HAS_EXCEPTIONS.
  if (NOT USE_LIBCXX)
    add_definitions(-D_HAS_EXCEPTIONS=0)
  endif()

  ## C4530, The code uses C++ exception handling,
  ## but /EHsc wasn't included in the compiler options
  ## https://docs.microsoft.com/en-us/cpp/build/reference/eh-exception-handling-model?view=msvc-170
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

# *****************************************************************************************
#           abseil-cpp Library
# *****************************************************************************************

set(ABSL_PROPAGATE_CXX_STD "ON" CACHE STRING "")
# override abseil-cpp options
include_directories(third_party/abseil-cpp-override)
# force win32 thread model for libc++ usage
if (MINGW AND USE_LIBCXX)
  # force non-inline emulated-tls
  add_definitions(-DABSL_CONSUME_DLL)
  # force c++11 thread model
  add_definitions(-DABSL_FORCE_THREAD_IDENTITY_MODE=ABSL_THREAD_IDENTITY_MODE_USE_CPP11)
endif()
if (ANDROID OR OHOS OR IOS)
  set(LIBRT "OFF" CACHE BOOL "")
  set(HAVE_LIB_RT "OFF" CACHE BOOL "")
endif()
if (ABSL_BUILD_TESTING)
  set(ABSL_USE_EXTERNAL_GOOGLETEST "ON" CACHE BOOL "")
  add_library(absl_gtest STATIC
    third_party/googletest/googletest/src/gtest-all.cc)
  target_include_directories(absl_gtest PRIVATE
    third_party/googletest/googletest)
  target_include_directories(absl_gtest PUBLIC
    third_party/googletest/googletest/include)
  add_library(absl_gtest_main STATIC
    third_party/googletest/googletest/src/gtest_main.cc)
  add_library(GTest::gtest ALIAS absl_gtest)
  add_library(GTest::gtest_main ALIAS absl_gtest_main)

  add_library(absl_gmock STATIC
    third_party/googletest/googlemock/src/gmock-all.cc)
  target_include_directories(absl_gmock PRIVATE
    third_party/googletest/googlemock)
  target_include_directories(absl_gmock PUBLIC
    third_party/googletest/googletest/include
    third_party/googletest/googlemock/include)
  add_library(absl_gmock_main STATIC
    third_party/googletest/googlemock/src/gmock_main.cc)
  add_library(GTest::gmock ALIAS absl_gmock)
  add_library(GTest::gmock_main ALIAS absl_gmock_main)
  target_link_libraries(absl_gmock PUBLIC absl_gtest)

  target_link_libraries(absl_gtest_main PUBLIC absl_gtest absl_gmock)
  target_link_libraries(absl_gmock_main PUBLIC absl_gtest absl_gmock)

  set(GTEST_LIBRARY absl_gtest)
  set(GTEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/googletest)
  set(GTEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/googletest/include)
  set(GTEST_MAIN_LIBRARY absl_gtest_main)
  set(GMOCK_LIBRARY absl_gmock)
  set(GMOCK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/googlemock/include)
  set(GMOCK_MAIN_LIBRARY absl_gmock_main)
endif()
add_subdirectory(third_party/abseil-cpp)

list(APPEND YASS_APP_FEATURES "abseil-cpp")
set(SUPPORT_LIBS
  absl::base
  absl::core_headers
  absl::flat_hash_map
  absl::flat_hash_set
  absl::flags
  absl::flags_parse
  absl::flags_marshalling
  absl::synchronization
  absl::stacktrace
  absl::symbolize
  absl::status
  absl::statusor
  absl::strings
  absl::time
  absl::optional
  absl::failure_signal_handler
  ${SUPPORT_LIBS}
  )

# *****************************************************************************************
#           tcmalloc Library
# *****************************************************************************************

# only support platform: linux
if (USE_TCMALLOC)
  if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(WARNING "tcmalloc: only Linux is supported, disabling...")
    set(USE_TCMALLOC OFF)
  endif()
  if (OS_X86)
    message(WARNING "tcmalloc: 32-bit x86 is not supported, disabling...")
    set(USE_TCMALLOC OFF)
  endif()
  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "mips" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "mips64" OR
      CMAKE_SYSTEM_PROCESSOR STREQUAL "mipsel" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "mips64el")
    message(WARNING "tcmalloc: mips/mips64 is not supported, disabling...")
    set(USE_TCMALLOC OFF)
  endif()
  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "armhf" OR
      CMAKE_SYSTEM_PROCESSOR STREQUAL "armel")
    message(WARNING "tcmalloc: 32-bit arm is not supported, disabling...")
    set(USE_TCMALLOC OFF)
  endif()
endif()

# tunings:
# default pages: nop
# large pages: -DTCMALLOC_LARGE_PAGES
# 256k pages: -DTCMALLOC_256K_PAGES
# small pages: -DTCMALLOC_SMALL_BUT_SLOW
# numa pages: -DTCMALLOC_NUMA_AWARE
if (USE_TCMALLOC)
  message(STATUS "Compiling with bundled tcmalloc support")

  add_subdirectory(third_party/tcmalloc)

  list(APPEND YASS_APP_FEATURES "tcmalloc")
  set(SUPPORT_DEFINITIONS
    HAVE_TCMALLOC
    ${SUPPORT_DEFINITIONS}
  )
  set(SUPPORT_LIBS
    tcmalloc
    ${SUPPORT_LIBS}
    )
  # silence lto warning on gcc: tcmalloc_sampler
  if (COMPILER_GCC AND LTO)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0)
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wno-error=odr")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wno-error=odr")
    else()
      set(CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_SHARED_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wno-lto-type-mismatch")
      set(CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE} "${CMAKE_EXE_LINKER_FLAGS_${upper_CMAKE_BUILD_TYPE}} -Wno-lto-type-mismatch")
    endif()
  endif()
endif()

# *****************************************************************************************
#           icu Library
# *****************************************************************************************

if (USE_ICU)
  message(STATUS "Compiling with bundled icu library")
  add_subdirectory(third_party/icu)
  list(APPEND YASS_APP_FEATURES "icu")
  set(SUPPORT_DEFINITIONS
    HAVE_ICU
    ${SUPPORT_DEFINITIONS}
  )
  set(SUPPORT_LIBS icuuc icui18n ${SUPPORT_LIBS})
endif()

# *****************************************************************************************
#           re2 Library
# *****************************************************************************************

message(STATUS "Compiling with bundled re2 library")
add_subdirectory(third_party)
list(APPEND YASS_APP_FEATURES "re2")
set(SUPPORT_LIBS re2 ${SUPPORT_LIBS})

# *****************************************************************************************
#           googleurl Library
# *****************************************************************************************

message(STATUS "Compiling with bundled googleurl library")
list(APPEND YASS_APP_FEATURES "googleurl")
set(SUPPORT_LIBS url ${SUPPORT_LIBS})

# *****************************************************************************************
#           crashpad Library
# *****************************************************************************************

set(_CRASHPAD_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third_party/crashpad/crashpad/out/Default-${MSVC_C_ARCHITECTURE_ID}")
set(_CRASHPAD_BINARY_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third_party/crashpad/crashpad/out/Binary-${MSVC_C_ARCHITECTURE_ID}")
if (MSVC AND MSVC_CRT_LINKAGE STREQUAL "dynamic" AND NOT ALLOW_XP AND ${CMAKE_BUILD_TYPE} MATCHES Release AND EXISTS "${_CRASHPAD_PREFIX}/obj/client/client.lib")
  message(STATUS "Compiling with bundled crashpad client library")
  add_library(crashpad_common STATIC IMPORTED)
  set_property(TARGET crashpad_common PROPERTY
               IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/client/common.lib")
  add_library(crashpad_util STATIC IMPORTED)
  set_property(TARGET crashpad_util PROPERTY
               IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/util/util.lib")
  add_library(crashpad_client STATIC IMPORTED)
  set_property(TARGET crashpad_client PROPERTY
               IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/client/client.lib")
  add_library(mini_chromium_base STATIC IMPORTED)
  set_property(TARGET mini_chromium_base PROPERTY
               IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/third_party/mini_chromium/mini_chromium/base/base.lib")

  add_library(yass_crashpad STATIC
    src/crashpad_helper.cpp
    src/crashpad_helper.hpp
  )
  target_compile_definitions(yass_crashpad PUBLIC HAVE_CRASHPAD)
  target_include_directories(yass_crashpad PRIVATE
    "${_CRASHPAD_PREFIX}/gen"
    third_party/crashpad/crashpad/third_party/mini_chromium/mini_chromium
    third_party/crashpad/crashpad
    src
  )
  target_link_libraries(yass_crashpad PRIVATE crashpad_client crashpad_common crashpad_util mini_chromium_base)
  list(APPEND YASS_APP_FEATURES "crashpad")
  set(SUPPORT_LIBS yass_crashpad ${SUPPORT_LIBS})

  file(COPY "${_CRASHPAD_BINARY_PREFIX}/crashpad_handler.exe" DESTINATION ${CMAKE_BINARY_DIR})

  install(FILES "${_CRASHPAD_BINARY_PREFIX}/crashpad_handler.exe" DESTINATION bin)
endif()

if (IOS)
  if (SDK_NAME STREQUAL iphoneos)
    set(_CRASHPAD_OS "-ios")
    set(_CRASHPAD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
  else()
    set(_CRASHPAD_OS "-ios-simulator")
    set(_CRASHPAD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
  endif()
elseif (ANDROID)
  set(_CRASHPAD_OS "-android")
  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64-v8a")
    set(_CRASHPAD_ARCH "arm64")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
    set(_CRASHPAD_ARCH "arm")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
    set(_CRASHPAD_ARCH "x86")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(_CRASHPAD_ARCH "x64")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
    set(_CRASHPAD_ARCH "riscv64")
  else()
    message(WARNING "crashpad: unsupported android architecture ${CMAKE_SYSTEM_PROCESSOR}")
    set(_CRASHPAD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
  endif()
elseif (NOT OSX_CROSS_ARCHITECTURES_COUNT)
  set(_CRASHPAD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
elseif (OSX_CROSS_ARCHITECTURES_COUNT EQUAL 1)
  set(_CRASHPAD_ARCH "${CMAKE_OSX_ARCHITECTURES}")
else()
  set(_CRASHPAD_ARCH "mac_universal")
endif()
# mapping to gn arch syntax
if (_CRASHPAD_ARCH STREQUAL "x86_64" OR _CRASHPAD_ARCH STREQUAL "amd64")
  set(_CRASHPAD_ARCH "x64")
elseif (_CRASHPAD_ARCH STREQUAL "i386" OR _CRASHPAD_ARCH STREQUAL "i586" OR _CRASHPAD_ARCH STREQUAL "i686")
  set(_CRASHPAD_ARCH "x86")
elseif (_CRASHPAD_ARCH STREQUAL "aarch64")
  set(_CRASHPAD_ARCH "arm64")
endif()
set(_CRASHPAD_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third_party/crashpad/crashpad/out/Default-${_CRASHPAD_ARCH}${_CRASHPAD_OS}")
set(_CRASHPAD_BINARY_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third_party/crashpad/crashpad/out/Binary-${_CRASHPAD_ARCH}${_CRASHPAD_OS}")
if (UNIX AND (${CMAKE_BUILD_TYPE} MATCHES MinSizeRel OR ${CMAKE_BUILD_TYPE} MATCHES Release) AND EXISTS "${_CRASHPAD_PREFIX}/obj/client/libclient.a")
  message(STATUS "Compiling with bundled crashpad client library")
  add_library(crashpad_common STATIC IMPORTED)
  set_property(TARGET crashpad_common PROPERTY
               IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/client/libcommon.a")
  add_library(crashpad_util STATIC IMPORTED)
  set_property(TARGET crashpad_util PROPERTY
               IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/util/libutil.a")
  add_library(crashpad_client STATIC IMPORTED)
  set_property(TARGET crashpad_client PROPERTY
               IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/client/libclient.a")
  add_library(mini_chromium_base STATIC IMPORTED)
  set_property(TARGET mini_chromium_base PROPERTY
               IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/third_party/mini_chromium/mini_chromium/base/libbase.a")

  add_library(yass_crashpad STATIC
    src/crashpad_helper.cpp
    src/crashpad_helper.hpp
  )
  target_compile_definitions(yass_crashpad PUBLIC HAVE_CRASHPAD)
  target_include_directories(yass_crashpad PRIVATE
    "${_CRASHPAD_PREFIX}/gen"
    third_party/crashpad/crashpad/third_party/mini_chromium/mini_chromium
    third_party/crashpad/crashpad
    src
  )
  target_link_libraries(yass_crashpad PRIVATE crashpad_client crashpad_common crashpad_util mini_chromium_base)
  list(APPEND YASS_APP_FEATURES "crashpad")
  set(SUPPORT_LIBS yass_crashpad ${SUPPORT_LIBS})
  if (IOS)
    add_library(crashpad_handler_common STATIC IMPORTED)
    set_property(TARGET crashpad_handler_common PROPERTY
                 IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/handler/libcommon.a")
    target_link_libraries(yass_crashpad PRIVATE crashpad_handler_common)

    add_library(crashpad_minidump STATIC IMPORTED)
    set_property(TARGET crashpad_minidump PROPERTY
                 IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/minidump/libminidump.a")
    target_link_libraries(yass_crashpad PRIVATE crashpad_minidump)

    add_library(crashpad_snapshot STATIC IMPORTED)
    set_property(TARGET crashpad_snapshot PROPERTY
                 IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/snapshot/libsnapshot.a")
    target_link_libraries(yass_crashpad PRIVATE crashpad_snapshot)

    add_library(crashpad_snapshot_context STATIC IMPORTED)
    set_property(TARGET crashpad_snapshot_context PROPERTY
                 IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/snapshot/libcontext.a")
    target_link_libraries(yass_crashpad PRIVATE crashpad_snapshot_context)

    add_library(crashpad_util_net STATIC IMPORTED)
    set_property(TARGET crashpad_util_net PROPERTY
                 IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/util/libnet.a")
    target_link_libraries(yass_crashpad PRIVATE crashpad_util_net)

    target_link_libraries(yass_crashpad PRIVATE z)
  endif()
  if (APPLE)
    add_library(crashpad_util_mig STATIC IMPORTED)
    set_property(TARGET crashpad_util_mig PROPERTY
                 IMPORTED_LOCATION "${_CRASHPAD_PREFIX}/obj/util/libmig_output.a")
    target_link_libraries(yass_crashpad PRIVATE crashpad_util_mig bsm)
  endif()

  if (NOT IOS)
    file(COPY "${_CRASHPAD_BINARY_PREFIX}/crashpad_handler" DESTINATION ${CMAKE_BINARY_DIR})
    set(_CRASHPAD_BINARY "${CMAKE_BINARY_DIR}/crashpad_handler")
  endif()
endif()

# *****************************************************************************************
#           tun2proxy Library
# *****************************************************************************************

if (ANDROID)
  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64-v8a")
    set(_TUN2PROXY_TARGET "aarch64-linux-android")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
    set(_TUN2PROXY_TARGET "armv7-linux-androideabi")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
    set(_TUN2PROXY_TARGET "i686-linux-android")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(_TUN2PROXY_TARGET "x86_64-linux-android")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
    set(_TUN2PROXY_TARGET "riscv64-linux-android")
  else()
    message(FATAL_ERROR "tun2proxy: unsupported android architecture ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  set(TUN2PROXY_LIB third_party/tun2proxy/target/${_TUN2PROXY_TARGET}/release/libtun2proxy.so)
endif()

if (TUN2PROXY_LIB)
  file(COPY "${TUN2PROXY_LIB}" DESTINATION ${CMAKE_BINARY_DIR})
endif()

# *****************************************************************************************
#           boringssl Library
# *****************************************************************************************

# hacks to force win32 thread usage
if (MINGW AND USE_LIBCXX)
  set(PREVIOUS_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  set(PREVIOUS_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -U__MINGW32__")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U__MINGW32__")
endif()
add_subdirectory(third_party/boringssl/src)
# restore hacks
if (MINGW AND USE_LIBCXX)
  set(CMAKE_C_FLAGS "${PREVIOUS_CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${PREVIOUS_CMAKE_CXX_FLAGS}")
endif()
list(APPEND YASS_APP_FEATURES "boringssl")
set(SUPPORT_DEFINITIONS
  HAVE_BORINGSSL
  ${SUPPORT_DEFINITIONS}
)
set(SUPPORT_LIBS crypto ssl ${SUPPORT_LIBS})
foreach(source ${BCM_ASM_SOURCES})
  set_source_files_properties(${source} PROPERTIES GENERATED true)
endforeach()

set_target_properties(bssl PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(decrepit PROPERTIES EXCLUDE_FROM_ALL TRUE)

if (BORINGSSL_BUILD_TESTS)
  add_custom_target(check_boringssl
    COMMAND ${GO_EXECUTABLE} run util/all_tests.go -build-dir
            ${CMAKE_CURRENT_BINARY_DIR}/third_party/boringssl/src
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/boringssl/src
    DEPENDS all_tests
    USES_TERMINAL)
endif()

# *****************************************************************************************
#           mbedssl Library
# *****************************************************************************************
# pip install -U 'jsonschema<4.0'
# pip install -U jinja2

if (USE_SYSTEM_MBEDTLS)
  check_library_exists(mbedcrypto mbedtls_cipher_init "" HAVE_SYSTEM_MBEDTLS)
  if (NOT HAVE_SYSTEM_MBEDTLS)
    message(STATUS "System mbedtls not found, using bundled one")
    set(HAVE_SYSTEM_MBEDTLS FALSE)
  endif()
endif()

if (USE_SYSTEM_MBEDTLS AND USE_MBEDTLS)
  message(STATUS "Compiling with system mbedtls library")
  list(APPEND YASS_APP_FEATURES "system mbedtls")
  set(SUPPORT_DEFINITIONS
    HAVE_MBEDTLS
    ${SUPPORT_DEFINITIONS}
  )

  set(SUPPORT_LIBS mbedcrypto ${SUPPORT_LIBS})
elseif (USE_MBEDTLS)
  message(STATUS "Compiling with bundled mbedtls library")
  set(USE_SHARED_MBEDTLS_LIBRARY "OFF" CACHE STRING "")
  set(USE_STATIC_MBEDTLS_LIBRARY "ON" CACHE STRING "")
  set(ENABLE_TESTING "OFF" CACHE STRING "")
  set(ENABLE_PROGRAMS "OFF" CACHE STRING "")
  set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "")
  set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON CACHE BOOL "")
  set(INSTALL_MBEDTLS_HEADERS OFF CACHE BOOL "")
  if (OS_X86)
    message(STATUS "Compiling with mbedtls aesni support (x86)")
    set(ENABLE_AESNI_C_X86 ON CACHE BOOL "")
  endif()

  add_subdirectory(third_party/mbedtls)

  list(APPEND YASS_APP_FEATURES "mbedtls")
  set(SUPPORT_DEFINITIONS
    HAVE_MBEDTLS
    ${SUPPORT_DEFINITIONS}
  )
  set(SUPPORT_LIBS mbedcrypto ${SUPPORT_LIBS})
endif()

# *****************************************************************************************
#           asio Library
# *****************************************************************************************

add_library(asio STATIC
  src/core/asio.hpp
  src/core/asio.cpp)
target_include_directories(asio PUBLIC
  src)
target_include_directories(asio PUBLIC
  third_party/googleurl/polyfills
  third_party/asio/asio/include)
target_compile_definitions(asio PUBLIC ASIO_STANDALONE)
target_compile_definitions(asio PUBLIC ASIO_NO_TYPEID)
target_compile_definitions(asio PUBLIC ASIO_NO_EXCEPTIONS)
target_compile_definitions(asio PUBLIC ASIO_NO_TS_EXECUTORS)
target_compile_definitions(asio PUBLIC ASIO_SEPARATE_COMPILATION)
target_compile_definitions(asio PUBLIC ASIO_NO_DEPRECATED)
# forward libcxx present
# we control the visiblity
target_compile_definitions(asio PUBLIC ASIO_DISABLE_VISIBILITY)
# both of asio and quiche use std::allocator<void>
if (MSVC AND NOT USE_LIBCXX)
  target_compile_definitions(asio PUBLIC _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)
  target_compile_definitions(asio PUBLIC _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)
endif()
target_link_libraries(asio PUBLIC
  absl::flags
  url
  crypto
  ssl)

if (APPLE)
  find_library(SECURITY_FRAMEWORK Security REQUIRED)
  target_link_libraries(asio PRIVATE ${SECURITY_FRAMEWORK})
endif()

if (USE_BUILTIN_CA_BUNDLE_CRT)
  set(ASIO_CA_BUNDLE_CRT_SRC)

  if (WIN32 AND NOT OS_AARCH64)
    enable_language(ASM_NASM)
    set(ASIO_CA_BUNDLE_CRT_SRC "third_party/ca-certificates/ca-bundle.crt.asm")
  endif()

  if (WIN32 AND OS_AARCH64)
    enable_language(ASM)
    if (COMPILER_CLANG)
      set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wno-unused-command-line-argument")
    endif()
    set(ASIO_CA_BUNDLE_CRT_SRC "third_party/ca-certificates/ca-bundle.crt.win.s")
  endif()

  if (OHOS OR ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    enable_language(ASM)
    if (COMPILER_CLANG)
      set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wno-unused-command-line-argument")
    endif()
    set(ASIO_CA_BUNDLE_CRT_SRC "third_party/ca-certificates/ca-bundle.crt.s")
  endif()

  if (APPLE)
    enable_language(ASM)
    if (COMPILER_CLANG)
      set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wno-unused-command-line-argument")
    endif()
    set(ASIO_CA_BUNDLE_CRT_SRC "third_party/ca-certificates/ca-bundle.crt.apple.s")
  endif()

  if (NOT ASIO_CA_BUNDLE_CRT_SRC)
    message(FATAL_ERROR "builtin ca bundle crt missing implementation")
  endif()

  add_library(asio_ca_bundle_crt OBJECT ${ASIO_CA_BUNDLE_CRT_SRC})
  target_compile_definitions(asio_ca_bundle_crt PUBLIC HAVE_BUILTIN_CA_BUNDLE_CRT=1)
  target_include_directories(asio_ca_bundle_crt PRIVATE third_party/ca-certificates)

  list(APPEND YASS_APP_FEATURES "builtin ca-bundle")
  target_link_libraries(asio PRIVATE asio_ca_bundle_crt)
endif()

list(APPEND YASS_APP_FEATURES "asio")
set(SUPPORT_LIBS asio ${SUPPORT_LIBS})

# *****************************************************************************************
#           json Library
# *****************************************************************************************


if (USE_SYSTEM_JSON)
  check_cxx_source_compiles ("
#include <nlohmann/json.hpp>
int main() {}
" HAVE_SYSTEM_JSON)
  if (NOT HAVE_SYSTEM_JSON)
    message(STATUS "System cares not found, using bundled one")
    set(USE_SYSTEM_JSON FALSE)
  endif()
endif()

if (USE_SYSTEM_JSON)
  message(STATUS "Compiling with system nlohmann json library")
  list(APPEND YASS_APP_FEATURES "system nlohmann json")
else()
  message(STATUS "Compiling with bundled nlohmann json library")
  list(APPEND YASS_APP_FEATURES "nlohmann json")
  set(SUPPORT_INCLUDE_DIRS
    third_party/json/include
    ${SUPPORT_INCLUDE_DIRS}
  )
endif()

# *****************************************************************************************
#           googletest Library
# *****************************************************************************************

# for forward declaration
include_directories(third_party/googletest/googletest/include)
include_directories(third_party/googletest/googlemock/include)
if (BUILD_TESTS)
  # Add minimal googletest targets. The provided one has many side-effects, and
  # googletest has a very straightforward build.
  add_library(yass_gtest STATIC
    third_party/googletest/googletest/src/gtest-all.cc
    third_party/googletest/googlemock/src/gmock-all.cc)
  target_include_directories(yass_gtest PUBLIC
    third_party/googletest/googletest
    third_party/googletest/googlemock)

  # absl part support requires re2
  target_compile_definitions(yass_gtest PUBLIC GTEST_HAS_ABSL=1)
  target_link_libraries(yass_gtest PUBLIC
    absl::failure_signal_handler
    absl::stacktrace
    absl::symbolize
    absl::flags_parse
    absl::flags_reflection
    absl::flags_usage
    absl::strings
    absl::any
    absl::optional
    absl::variant
    re2::re2
  )

  if(COMPILER_MSVC)
    # VS2019,VS2022 specific: storing 32-bit float result in memory, possible loss of performance
    target_compile_options(yass_gtest PRIVATE "/wd4738")
  endif()
endif()

# *****************************************************************************************
#           google benchmark Library
# *****************************************************************************************
if (BUILD_BENCHMARKS)
  set(BENCHMARK_ENABLE_TESTING "OFF" CACHE STRING "")
  set(BENCHMARK_ENABLE_EXCEPTIONS "OFF" CACHE STRING "")
  set(BENCHMARK_ENABLE_LTO "OFF" CACHE STRING "")
  set(BENCHMARK_USE_LIBCXX "OFF" CACHE STRING "")
  set(BENCHMARK_ENABLE_WERROR "OFF" CACHE STRING "")
  set(BENCHMARK_FORCE_WERROR "OFF" CACHE STRING "")
  set(BENCHMARK_BUILD_32_BITS "OFF" CACHE STRING "")
  set(BENCHMARK_ENABLE_INSTALL "OFF" CACHE STRING "")
  set(BENCHMARK_ENABLE_DOXYGEN "OFF" CACHE STRING "")
  set(BENCHMARK_INSTALL_DOCS "OFF" CACHE STRING "")
  set(BENCHMARK_DOWNLOAD_DEPENDENCIES "OFF" CACHE STRING "")
  set(BENCHMARK_ENABLE_GTEST_TESTS "OFF" CACHE STRING "")
  set(BENCHMARK_USE_BUNDLED_GTEST "OFF" CACHE STRING "")
  set(BENCHMARK_ENABLE_ASSEMBLY_TESTS "OFF" CACHE STRING "")
  # by-pass linkage issue with libc++
  set(HAVE_STD_REGEX "ON" CACHE STRING "")
  set(HAVE_STEADY_CLOCK "ON" CACHE STRING "")

check_c_source_compiles("
#include <regex.h>
#include <stddef.h>
int main() {
  const char* str = \"test0159\";
  regex_t re;
  int ec = regcomp(&re, \"^[a-z]+[0-9]+$\", REG_EXTENDED | REG_NOSUB);
  if (ec != 0) {
    return ec;
  }
  int ret = regexec(&re, str, 0, NULL, 0) ? -1 : 0;
  regfree(&re);
  return ret;
} " HAVE_POSIX_REGEX_C)
  if (HAVE_POSIX_REGEX_C)
    set(HAVE_POSIX_REGEX "ON" CACHE STRING "")
  endif()

check_c_source_compiles("
#include <gnuregex.h>
#include <stddef.h>
int main() {
  const char* str = \"test0159\";
  regex_t re;
  int ec = regcomp(&re, \"^[a-z]+[0-9]+$\", REG_EXTENDED | REG_NOSUB);
  if (ec != 0) {
    return ec;
  }
  return regexec(&re, str, 0, NULL, 0) ? -1 : 0;
}
} " HAVE_GNU_POSIX_REGEX_C)
  if (HAVE_GNU_POSIX_REGEX_C)
    set(HAVE_GNU_POSIX_REGEX "ON" CACHE STRING "")
  endif()

  add_subdirectory(third_party/benchmark)
  # fix a typo in re.h
  target_compile_definitions(benchmark PRIVATE BENCHMARK_HAVE_STD_REGEX)
endif()

# *****************************************************************************************
#           Protobuf Library
# *****************************************************************************************

set(protobuf_INSTALL "OFF" CACHE STRING "")
set(protobuf_DISABLE_RTTI "ON" CACHE STRING "")
if (MSVC_CRT_LINKAGE STREQUAL "dynamic")
  set(protobuf_MSVC_STATIC_RUNTIME "OFF" CACHE STRING "")
else()
  set(protobuf_MSVC_STATIC_RUNTIME "ON" CACHE STRING "")
endif()
set(protobuf_BUILD_TESTS "OFF" CACHE STRING "")
set(protobuf_WITH_ZLIB "OFF" CACHE STRING "")
set(protobuf_BUILD_SHARED_LIBS "OFF" CACHE STRING "")
add_subdirectory(third_party/protobuf)
# required because it is not bound with libproto/libproto-lite targets
list(APPEND YASS_APP_FEATURES "protobuf")
set(SUPPORT_DEFINITIONS
  GOOGLE_PROTOBUF_NO_RTTI=1
  PROTOBUF_USE_EXCEPTIONS=0
  ${SUPPORT_DEFINITIONS}
)

# fix protoc's path
set_target_properties(protoc PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

# exclude non-native protoc from being build
set_target_properties(protoc PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(libprotoc PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(libprotobuf PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(libprotobuf-lite PROPERTIES EXCLUDE_FROM_ALL TRUE)

include(Protobuf)
add_protoc(protoc yass)

# *****************************************************************************************
#           Zlib Library
# *****************************************************************************************

if (USE_SYSTEM_ZLIB)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(ZLIB zlib)
  endif()
  if(NOT ZLIB_FOUND)
    message(STATUS "System zlib not found, using bundled one")
    set(USE_SYSTEM_ZLIB FALSE)
  endif()
endif()

if (USE_SYSTEM_ZLIB)
  message(STATUS "Compiling with system zlib")
  include_directories(${ZLIB_INCLUDE_DIRS})
  link_directories(${ZLIB_LIBRARY_DIRS})
  link_libraries(${ZLIB_LIBRARIES})
  list(APPEND YASS_APP_FEATURES "system zlib")
else()
  message(STATUS "Compiling with bundled zlib")
  add_subdirectory(third_party/zlib)
  list(APPEND YASS_APP_FEATURES "zlib")
  set(SUPPORT_LIBS zlib ${SUPPORT_LIBS})
endif()

# *****************************************************************************************
#           c-ares Library
# *****************************************************************************************

if (USE_SYSTEM_CARES)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(CARES libcares)
  endif()
  if(NOT CARES_FOUND)
    message(STATUS "System cares not found, using bundled one")
    set(USE_SYSTEM_CARES FALSE)
  endif()
endif()

if (USE_SYSTEM_CARES AND USE_CARES)
  message(STATUS "Compiling with system cares")
  list(APPEND YASS_APP_FEATURES "system c-ares")
  set(SUPPORT_DEFINITIONS
    HAVE_C_ARES
    ${SUPPORT_DEFINITIONS}
  )
  include_directories(${CARES_INCLUDE_DIRS})
  link_directories(${CARES_LIBRARY_DIRS})
  link_libraries(${CARES_LIBRARIES})
elseif (USE_CARES)
  message(STATUS "Compiling with bundled cares")
  list(APPEND YASS_APP_FEATURES "c-ares")
  set(SUPPORT_DEFINITIONS
    HAVE_C_ARES
    ${SUPPORT_DEFINITIONS}
  )
  set(CARES_BUILD_TOOLS OFF CACHE BOOL "")
  set(CARES_SHARED OFF CACHE BOOL "")
  set(CARES_STATIC ON CACHE BOOL "")
  set(CARES_STATIC_PIC ON CACHE BOOL "")
  set(CARES_BUILD_TESTS OFF CACHE BOOL "")
  set(CARES_INSTALL OFF CACHE BOOL "")
  add_subdirectory(third_party/c-ares)

  set(SUPPORT_LIBS c-ares ${SUPPORT_LIBS})
endif()

# *****************************************************************************************
#           Nghttp2 Library
# *****************************************************************************************

if (USE_SYSTEM_NGHTTP2)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(NGHTTP2 libnghttp2)
  endif()
  if(NOT NGHTTP2_FOUND)
    message(STATUS "System nghttp2 not found, using bundled one")
    set(USE_SYSTEM_NGHTTP2 FALSE)
  endif()
endif()

if (USE_SYSTEM_NGHTTP2 AND USE_NGHTTP2)
  message(STATUS "Compiling with system libnghttp2")
  list(APPEND YASS_APP_FEATURES "system nghttp2")
  set(SUPPORT_DEFINITIONS
    HAVE_NGHTTP2
    ${SUPPORT_DEFINITIONS}
  )
  include_directories(${NGHTTP2_INCLUDE_DIRS})
  link_directories(${NGHTTP2_LIBRARY_DIRS})
  link_libraries(${NGHTTP2_LIBRARIES})
elseif (USE_NGHTTP2)
  message(STATUS "Compiling with bundled libnghttp2")
  set(ENABLE_APP OFF CACHE BOOL "")
  set(ENABLE_HPACK_TOOLS OFF CACHE BOOL "")
  set(ENABLE_EXAMPLES OFF CACHE BOOL "")
  set(ENABLE_LIB_ONLY ON CACHE BOOL "")
  set(ENABLE_SHARED_LIB OFF CACHE BOOL "")
  set(ENABLE_STATIC_LIB ON CACHE BOOL "")
  set(ENABLE_DOC OFF CACHE BOOL "")
  set(DISABLE_INSTALL ON CACHE BOOL "")
  add_subdirectory(third_party/nghttp2)
  if (COMPILER_CLANG)
    target_compile_options(nghttp2_static PRIVATE -Wno-cast-qual)
    target_compile_options(nghttp2_static PRIVATE -Wno-implicit-fallthrough)
    if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 16.0)
      target_compile_options(nghttp2_static PRIVATE -Wno-unsafe-buffer-usage)
    endif()
    target_compile_options(nghttp2_static PRIVATE -Wno-documentation)
    target_compile_options(nghttp2_static PRIVATE -Wno-documentation-unknown-command)
    target_compile_options(nghttp2_static PRIVATE -Wno-switch-enum)
    target_compile_options(nghttp2_static PRIVATE -Wno-shorten-64-to-32)
    target_compile_options(nghttp2_static PRIVATE -Wno-extra-semi-stmt)
    target_compile_options(nghttp2_static PRIVATE -Wno-covered-switch-default)
  endif()
  set(NGHTTP2_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nghttp2/lib/includes
    ${CMAKE_CURRENT_BINARY_DIR}/third_party/nghttp2/lib/includes
    )
  set(NGHTTP2_LIBRARIES nghttp2_static)
  list(APPEND YASS_APP_FEATURES "nghttp2")
  set(SUPPORT_DEFINITIONS
    HAVE_NGHTTP2
    ${SUPPORT_DEFINITIONS}
  )
  set(SUPPORT_INCLUDE_DIRS
    ${NGHTTP2_INCLUDE_DIRS}
    ${SUPPORT_INCLUDE_DIRS}
  )
  set(SUPPORT_LIBS
    nghttp2_static
    ${SUPPORT_LIBS}
  )
endif()

# *****************************************************************************************
#           QUICHE Library
# *****************************************************************************************

# include both of googleurl and quiche
# TODO:Uncomment the below line out
add_subdirectory(third_party/quiche)
list(APPEND YASS_APP_FEATURES "quiche")
set(SUPPORT_DEFINITIONS
  HAVE_QUICHE
  ${SUPPORT_DEFINITIONS}
)
set(SUPPORT_LIBS url quiche ${SUPPORT_LIBS})

# *****************************************************************************************
#           http-parser Library
# *****************************************************************************************

if (USE_SYSTEM_HTTP_PARSER AND NOT USE_BALSA_HTTP_PARSER)
  check_library_exists(http_parser http_parser_init "" HAVE_HTTP_PARSER_INIT)
  if (NOT HAVE_HTTP_PARSER_INIT)
    message(STATUS "System http_parser not found, using bundled one")
    set(USE_SYSTEM_HTTP_PARSER FALSE)
  endif()
endif()

if (USE_SYSTEM_HTTP_PARSER AND NOT USE_BALSA_HTTP_PARSER)
  message(STATUS "Compiling with system http-parser")
  list(APPEND YASS_APP_FEATURES "system http parser")
  set(SUPPORT_LIBS http_parser ${SUPPORT_LIBS})
elseif(NOT USE_BALSA_HTTP_PARSER)
  message(STATUS "Compiling with bundled http-parser")
  add_library(http_parser STATIC
    third_party/http_parser/http_parser.h
    third_party/http_parser/http_parser.c
    )
  target_include_directories(http_parser PUBLIC third_party/http_parser)
  list(APPEND YASS_APP_FEATURES "nodejs http parser")
  set(SUPPORT_LIBS http_parser ${SUPPORT_LIBS})
endif()

if (USE_BALSA_HTTP_PARSER)
  message(STATUS "Compiling with balsa http parser")
  list(APPEND YASS_APP_FEATURES "balsa http parser")
  set(SUPPORT_DEFINITIONS
    HAVE_BALSA_HTTP_PARSER
    ${SUPPORT_DEFINITIONS}
  )
  set(SUPPORT_LIBS balsa ${SUPPORT_LIBS})
endif()

# *****************************************************************************************
#           snappy Library
# *****************************************************************************************

if (USE_LEVELDB)
  message(STATUS "Compiling with bundled snappy library")
  add_subdirectory(third_party/snappy)
  include_directories(third_party/snappy/src)
  list(APPEND YASS_APP_FEATURES "snappy")
endif()

# *****************************************************************************************
#           zstd Library
# *****************************************************************************************

if (USE_LEVELDB)
  message(STATUS "Compiling with bundled zstd library")
  set(ZSTD_BUILD_PROGRAMS "OFF" CACHE STRING "")
  set(ZSTD_BUILD_CONTRIB "OFF" CACHE STRING "")
  set(ZSTD_BUILD_TESTS "OFF" CACHE STRING "")
  set(ZSTD_BUILD_STATIC "ON" CACHE STRING "")
  set(ZSTD_BUILD_SHARED "OFF" CACHE STRING "")
  set(ZSTD_PROGRAMS_LINK_SHARED "OFF" CACHE STRING "")
  set(ZSTD_INSTALL "OFF" CACHE STRING "")
  set(ZSTD_LEGACY_SUPPORT "OFF" CACHE STRING "")
  add_subdirectory(third_party/zstd/build/cmake)
  include_directories(third_party/zstd/lib)
  list(APPEND YASS_APP_FEATURES "zstd")
endif()

# *****************************************************************************************
#           leveldb Library
# *****************************************************************************************

if (USE_LEVELDB)
  message(STATUS "Compiling with bundled leveldb library")
  set(LEVELDB_BUILD_UTILS "OFF" CACHE STRING "")
  set(LEVELDB_BUILD_TESTS "OFF" CACHE STRING "")
  set(LEVELDB_BUILD_BENCHMARKS "OFF" CACHE STRING "")
  set(LEVELDB_INSTALL "OFF" CACHE STRING "")
  set(HAVE_CRC32C 0)
  set(HAVE_SNAPPY 1)
  set(HAVE_ZSTD 1)
  set(HAVE_TCMALLOC 0)
  add_subdirectory(third_party/leveldb)
  list(APPEND YASS_APP_FEATURES "leveldb")
  set(SUPPORT_DEFINITIONS
    HAVE_LEVELDB
    ${SUPPORT_DEFINITIONS}
  )
  set(SUPPORT_LIBS leveldb ${SUPPORT_LIBS})
endif()

# *****************************************************************************************
#           sqlite3 Library
# *****************************************************************************************

if (USE_SQLITE)
  message(STATUS "Compiling with bundled sqlite3 library")
  add_subdirectory(third_party/sqlite)
  list(APPEND YASS_APP_FEATURES "sqlite3")
  set(SUPPORT_DEFINITIONS
    HAVE_SQLITE
    ${SUPPORT_DEFINITIONS}
  )
  set(SUPPORT_LIBS sqlite ${SUPPORT_LIBS})
endif()

# *****************************************************************************************
#           Compiler warning flags
# *****************************************************************************************

if (COMPILER_GCC OR COMPILER_CLANG)
  # Note clang-cl is odd and sets both CLANG and MSVC. We base our configuration
  # primarily on our normal Clang one.
  set(C_CXX_FLAGS "-Werror -Wformat=2 -Wsign-compare -Wmissing-field-initializers -Wwrite-strings -Wvla -Wno-shadow")
  if (COMPILER_APPLE_CLANG)
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wno-error=shorten-64-to-32")
  endif()

  if (MSVC)
    # clang-cl sets different default warnings than clang. It also treats -Wall
    # as -Weverything, to match MSVC. Instead -W3 is the alias for -Wall.
    # See http://llvm.org/viewvc/llvm-project?view=revision&revision=319116
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -W3 -Wno-unused-parameter")
  elseif (ENABLE_CLANG_TIDY)
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -fno-common")
  else()
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wall -fno-common")
  endif()

  # honor it. Suppress it here to compensate. See https://crbug.com/772117.
  if (COMPILER_GCC AND USE_LIBCXX)
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wno-deprecated-declarations")
  endif()

  if(COMPILER_CLANG)
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wnewline-eof -fcolor-diagnostics")
  else()
    # GCC (at least 4.8.4) has a bug where it'll find unreachable free() calls
    # and declare that the code is trying to free a stack pointer.
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wno-free-nonheap-object")
  endif()

  if (COMPILER_CLANG OR CMAKE_C_COMPILER_VERSION VERSION_GREATER "7.0.0")
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wimplicit-fallthrough")
  endif()

  if (COMPILER_CLANG OR CMAKE_C_COMPILER_VERSION VERSION_GREATER "16.0.0")
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wextra-semi")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS} -Wmissing-prototypes -Wold-style-definition -Wstrict-prototypes")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_CXX_FLAGS} -Wmissing-declarations")

  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-warning-option")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option")
  endif()

  # In GCC, -Wmissing-declarations is the C++ spelling of -Wmissing-prototypes
  # and using the wrong one is an error. In Clang, -Wmissing-prototypes is the
  # spelling for both and -Wmissing-declarations is some other warning.
  #
  # https://gcc.gnu.org/onlinedocs/gcc-7.1.0/gcc/Warning-Options.html#Warning-Options
  # https://clang.llvm.org/docs/DiagnosticsReference.html#wmissing-prototypes
  # https://clang.llvm.org/docs/DiagnosticsReference.html#wmissing-declarations
  if (COMPILER_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmissing-prototypes")
  endif()

  ## extra warnings
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wextra")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")

  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wno-unused-parameter")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-inconsistent-missing-override")
  endif()

  # required by asio
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-inconsistent-missing-override -Wno-unused-private-field")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override -Wno-unused-private-field")
  endif()

  # ignore unknown prgramas, works for both of gcc and clang
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")

  # ignore complains from clang-cl and mfc/afx
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-local-typedef")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedef")
  endif()

  # Lots of third-party libraries have unused variables. Instead of
  # suppressing them individually, we just blanket suppress them here.
  if (COMPILER_CLANG OR COMPILER_GCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-variable")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable")
  endif()

  # Similarly, we're not going to fix all the C++11 narrowing issues in
  # third-party libraries.
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-c++11-narrowing")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++11-narrowing")
  endif()

  # Disabled for similar reasons as -Wunused-variable
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-but-set-variable")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable")
  endif()

  # See comment for -Wno-c++11-narrowing.
  if (COMPILER_GCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-narrowing")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing")
  endif()

  # -Wunused-local-typedefs is broken in gcc,
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63872
  if (COMPILER_GCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-local-typedefs")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs")
  endif()

  # Don't warn about "maybe" uninitialized. Clang doesn't include this
  # in -Wall but gcc does, and it gives false positives.
  if (COMPILER_GCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-maybe-uninitialized")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized")
  endif()

  # -Wcomment gives too many false positives in the case a
  # backslash ended comment line is followed by a new line of
  # comments
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61638
  if (COMPILER_GCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-comments")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-comments")
  endif()

  # "struct foo f = {0};"
  # Unused function parameters.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-field-initializers -Wno-unused-parameter")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers -Wno-unused-parameter")

  # TODO(thakis): This used to be implied by -Wno-unused-function,
  # which we no longer use. Check if it makes sense to remove
  # this as well. http://crbug.com/316352
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unneeded-internal-declaration")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unneeded-internal-declaration")
  endif()

  # Thread safety analysis. See base/thread_annotations.h and
  # https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
  if (COMPILER_CLANG AND NOT CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wthread-safety")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wthread-safety")
  endif()

  ## When compiling Objective-C, warns if a method is used whose
  ## availability is newer than the deployment target.
  if (APPLE AND COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunguarded-availability")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunguarded-availability")
  endif()

  # this may happens
  if (COMPILER_CLANG)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wno-unused-command-line-argument")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wno-unused-command-line-argument")
  endif()

  if (COMPILER_GCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-alloc-size-larger-than")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-alloc-size-larger-than")
  endif()

  if (APPLE AND COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=deprecated-declarations")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=deprecated-declarations")
  endif()

  if (COMPILER_GCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11.0)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-stringop-overread -Wno-stringop-overflow -Wno-array-bounds")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overread -Wno-stringop-overflow -Wno-array-bounds")
  endif()

  # silence some mingw compiler warnings
  if (WIN32 AND NOT MSVC AND COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
  endif()
elseif (MSVC)
  set(MSVC_DISABLED_WARNINGS_LIST
      "C4061" # enumerator 'identifier' in switch of enum 'enumeration' is not
              # explicitly handled by a case label
              # Disable this because it flags even when there is a default.
      "C4100" # 'exarg' : unreferenced formal parameter
      "C4127" # conditional expression is constant
      "C4200" # nonstandard extension used : zero-sized array in
              # struct/union.
      "C4204" # nonstandard extension used: non-constant aggregate initializer
      "C4221" # nonstandard extension used : 'identifier' : cannot be
              # initialized using address of automatic variable
      "C4242" # 'function' : conversion from 'int' to 'uint8_t',
              # possible loss of data
      "C4244" # 'function' : conversion from 'int' to 'uint8_t',
              # possible loss of data
      "C4267" # conversion from 'size_t' to 'int', possible loss of data
      "C4371" # layout of class may have changed from a previous version of the
              # compiler due to better packing of member '...'
      "C4388" # signed/unsigned mismatch
      "C4296" # '>=' : expression is always true
      "C4350" # behavior change: 'std::_Wrap_alloc...'
      "C4365" # '=' : conversion from 'size_t' to 'int',
              # signed/unsigned mismatch
      "C4389" # '!=' : signed/unsigned mismatch
      "C4464" # relative include path contains '..'
      "C4510" # 'argument' : default constructor could not be generated
      "C4512" # 'argument' : assignment operator could not be generated
      "C4514" # 'function': unreferenced inline function has been removed
      "C4548" # expression before comma has no effect; expected expression with
              # side-effect" caused by FD_* macros.
      "C4610" # struct 'argument' can never be instantiated - user defined
              # constructor required.
      "C4623" # default constructor was implicitly defined as deleted
      "C4625" # copy constructor could not be generated because a base class
              # copy constructor is inaccessible or deleted
      "C4626" # assignment operator could not be generated because a base class
              # assignment operator is inaccessible or deleted
      "C4628" # digraphs not supported with -Ze
      "C4668" # 'symbol' is not defined as a preprocessor macro, replacing with
              # '0' for 'directives'
              # Disable this because GTest uses it everywhere.
      "C4706" # assignment within conditional expression
      "C4710" # 'function': function not inlined
      "C4711" # function 'function' selected for inline expansion
      "C4746" # volatile access of '<expression>' is subject to /volatile:[iso|ms] setting;
              # consider using __iso_volatile_load/store intrinsic functions.
      "C4800" # 'int' : forcing value to bool 'true' or 'false'
              # (performance warning)
      "C4820" # 'bytes' bytes padding added after construct 'member_name'
      "C5026" # move constructor was implicitly defined as deleted
      "C5027" # move assignment operator was implicitly defined as deleted
      "C5045" # Compiler will insert Spectre mitigation for memory load if
              # /Qspectre switch specified
      "C4099" # PDB 'filename' was not found with 'object/library' or at 'path';
              # linking object as if no debug info
      "C4505" # 'function' : unreferenced local function has been removed
      "C4838" # conversion from 'type_1' to 'type_2' requires a narrowing conversion
      "C4996" # uses a function, class member, variable, or typedef that's marked deprecated.
      "C4324" # 'struct_name' : structure was padded due to __declspec(align())
      "C4619" # #pragma warning : there is no warning number 'number'
      "C4068" # The compiler ignored an unrecognized pragma.
      "C4355" # STL suppression starts here
      "C5220"
      "C5204"
      "C4355"
      "C4774"
      "C4987"
      "C4582"
      "C4583" # STL suppression ends here
      "C5039" # abseil-cpp suppresion starts here
      "C4946"
      "C4995"
      "C4868" # abseil-cpp suppresion ends here
      "C5246" # VC2019/2022 specific: 'member': the initialization of a subobject should be wrapped in braces
      "C4596" # VC2017 specific: illegal qualified name in member declaration
      "C4571" # VC2017 specific: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught"
      "C4986" # VC2015 specific: exception specification does not match previous declaration"
      "C4814" # VC2015 specific: in C++14 'constexpr' will not imply 'const'; consider explicitly specifying 'const'
      "C4702" # VC2015 specific: unreachable code
      "C4917" # VC2015,xp specific: a GUID can only be associated with a class, interface or namespace
      "C4457" # VS2019 specific: declaration of 'message' hides function parameter
      "C5039" # VS2019 specific: pointer or reference to potentially throwing function passed to extern C function under -EHc
      "C5262" # VS2022 specific: Use [[fallthrough]] when a break statement is intentionally omitted between cases
      "C5264" # VS2022 specific: 'const' variable is not used
      "C4310" # VS2019 specific: cast truncates constant value
      )
  set(MSVC_LEVEL4_WARNINGS_LIST
      # See https://connect.microsoft.com/VisualStudio/feedback/details/1217660/warning-c4265-when-using-functional-header
      "C4265" # class has virtual functions, but destructor is not virtual
      )
  string(REPLACE "C" " -wd" MSVC_DISABLED_WARNINGS_STR
                            ${MSVC_DISABLED_WARNINGS_LIST})
  string(REPLACE "C" " -w4" MSVC_LEVEL4_WARNINGS_STR
                            ${MSVC_LEVEL4_WARNINGS_LIST})

  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${MSVC_DISABLED_WARNINGS_STR} ${MSVC_LEVEL4_WARNINGS_STR}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MSVC_DISABLED_WARNINGS_STR} ${MSVC_LEVEL4_WARNINGS_STR}")

  # Currently, STL is not cleanly compiled with /Wall
  # VS 2017 and higher supports STL-only warning suppressions.
  # https://github.com/microsoft/STL/blob/main/stl/inc/yvals_core.h#L479
  # https://devblogs.microsoft.com/cppblog/stl-fixes-in-vs-2017-rtm/
  add_definitions(-D_STL_EXTRA_DISABLED_WARNINGS=5220\ 5204\ 4355\ 4774\ 4987\ 4582\ 4583)

  ## doesn't enable clang-tidy's -Wall and -Wx which is annoying
  if (NOT ENABLE_CLANG_TIDY)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -WX")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -WX")
  endif()
endif()

# *****************************************************************************************
#           Static Analysis
# *****************************************************************************************

set(ENABLE_CLANG_TIDY "OFF" CACHE STRING "enable clang tidy build")
set(CLANG_TIDY_EXECUTABLE "clang-tidy" CACHE STRING "path to clang-tidy executable")
if (ENABLE_CLANG_TIDY)
  set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}")
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}")
  set(CMAKE_OBJC_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}")
  set(CMAKE_OBJCXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}")
endif()

# *****************************************************************************************
#           Source code
# *****************************************************************************************

set(files
    src/network.cpp
    src/net/ssl_socket.cpp
    src/net/ssl_server_socket.cpp
    src/net/openssl_util.cpp
    src/config/config.cpp
    src/config/config_tls.cpp
    src/config/config_impl.cpp
    src/core/cipher.cpp
    src/core/hkdf_sha1.cpp
    src/core/hmac_sha1.cpp
    src/core/iobuf.cpp
    src/core/socks4_request_parser.cpp
    src/core/socks5_request_parser.cpp
    src/core/ss_request_parser.cpp
    src/core/process_utils.cpp
    src/core/rand_util.cpp
    src/core/logging.cpp
    src/core/utils.cpp
    src/core/utils_fs.cpp
    src/core/safe_strerror.cpp
    src/core/debug.cpp
    src/core/check_op.cpp
    src/core/check.cpp
    src/core/scoped_clear_last_error_win.cpp
    src/core/ref_counted.cpp
    src/core/base64.cpp
    src/core/thread_collision_warner.hpp
    src/core/thread_collision_warner.cpp
    src/crypto/aead_base_decrypter.cpp
    src/crypto/aead_base_encrypter.cpp
    src/crypto/aead_evp_decrypter.cpp
    src/crypto/aead_evp_encrypter.cpp
    src/crypto/aead_sodium_decrypter.cpp
    src/crypto/aead_sodium_encrypter.cpp
    src/crypto/aead_mbedtls_decrypter.cpp
    src/crypto/aead_mbedtls_encrypter.cpp
    src/crypto/aes_128_gcm_12_evp_decrypter.cpp
    src/crypto/aes_128_gcm_12_evp_encrypter.cpp
    src/crypto/aes_128_gcm_evp_decrypter.cpp
    src/crypto/aes_128_gcm_evp_encrypter.cpp
    src/crypto/aes_192_gcm_evp_decrypter.cpp
    src/crypto/aes_192_gcm_evp_encrypter.cpp
    src/crypto/aes_256_gcm_evp_decrypter.cpp
    src/crypto/aes_256_gcm_evp_encrypter.cpp
    src/crypto/aes_256_gcm_sodium_decrypter.cpp
    src/crypto/aes_256_gcm_sodium_encrypter.cpp
    src/crypto/chacha20_poly1305_evp_decrypter.cpp
    src/crypto/chacha20_poly1305_evp_encrypter.cpp
    src/crypto/chacha20_poly1305_sodium_decrypter.cpp
    src/crypto/chacha20_poly1305_sodium_encrypter.cpp
    src/crypto/crypter.cpp
    src/crypto/crypter_export.cpp
    src/crypto/decrypter.cpp
    src/crypto/encrypter.cpp
    src/crypto/xchacha20_poly1305_evp_decrypter.cpp
    src/crypto/xchacha20_poly1305_evp_encrypter.cpp
    src/crypto/xchacha20_poly1305_sodium_decrypter.cpp
    src/crypto/xchacha20_poly1305_sodium_encrypter.cpp
    src/crypto/mbedtls_common.cpp

    src/core/c-ares.cpp
    src/core/http_parser.cpp
    )

set(hfiles
    src/network.hpp
    src/net/ssl_socket.hpp
    src/net/ssl_server_socket.hpp
    src/net/net_errors.hpp
    src/net/net_error_list.hpp
    src/net/openssl_util.hpp
    src/config/config.hpp
    src/config/config_impl.hpp
    src/config/config_impl_local.hpp
    src/config/config_impl_windows.hpp
    src/core/cipher.hpp
    src/core/hkdf_sha1.hpp
    src/core/hmac_sha1.hpp
    src/core/iobuf.hpp
    src/core/socks4.hpp
    src/core/socks4_request.hpp
    src/core/socks4_request_parser.hpp
    src/core/socks5.hpp
    src/core/socks5_request.hpp
    src/core/socks5_request_parser.hpp
    src/core/ss.hpp
    src/core/ss_request.hpp
    src/core/ss_request_parser.hpp
    src/core/process_utils.hpp
    src/core/rand_util.hpp
    src/core/logging.hpp
    src/core/utils.hpp
    src/core/utils_fs.hpp
    src/core/safe_strerror.hpp
    src/core/debug.hpp
    src/core/check_op.hpp
    src/core/check.hpp
    src/core/atomic_ref_count.hpp
    src/core/ref_counted.hpp
    src/core/base64.hpp
    src/crypto/aead_base_decrypter.hpp
    src/crypto/aead_base_encrypter.hpp
    src/crypto/aead_evp_decrypter.hpp
    src/crypto/aead_evp_encrypter.hpp
    src/crypto/aead_sodium_decrypter.hpp
    src/crypto/aead_sodium_encrypter.hpp
    src/crypto/aead_mbedtls_decrypter.hpp
    src/crypto/aead_mbedtls_encrypter.hpp
    src/crypto/aes_128_gcm_12_evp_decrypter.hpp
    src/crypto/aes_128_gcm_12_evp_encrypter.hpp
    src/crypto/aes_128_gcm_evp_decrypter.hpp
    src/crypto/aes_128_gcm_evp_encrypter.hpp
    src/crypto/aes_192_gcm_evp_decrypter.hpp
    src/crypto/aes_192_gcm_evp_encrypter.hpp
    src/crypto/aes_256_gcm_evp_decrypter.hpp
    src/crypto/aes_256_gcm_evp_encrypter.hpp
    src/crypto/aes_256_gcm_sodium_decrypter.hpp
    src/crypto/aes_256_gcm_sodium_encrypter.hpp
    src/crypto/chacha20_poly1305_evp_decrypter.hpp
    src/crypto/chacha20_poly1305_evp_encrypter.hpp
    src/crypto/chacha20_poly1305_sodium_decrypter.hpp
    src/crypto/chacha20_poly1305_sodium_encrypter.hpp
    src/crypto/crypter.hpp
    src/crypto/crypter_export.hpp
    src/crypto/decrypter.hpp
    src/crypto/encrypter.hpp
    src/crypto/xchacha20_poly1305_evp_decrypter.hpp
    src/crypto/xchacha20_poly1305_evp_encrypter.hpp
    src/crypto/xchacha20_poly1305_sodium_decrypter.hpp
    src/crypto/xchacha20_poly1305_sodium_encrypter.hpp
    src/crypto/mbedtls_common.hpp

    src/core/c-ares.hpp
    src/core/http_parser.hpp
    )

if (WIN32)
  list(APPEND files src/core/rand_util_win.cpp)
  list(APPEND files src/core/utils_win.cpp)
elseif (APPLE AND COMPILER_CLANG)
  list(APPEND files src/config/config_impl_apple.hpp)
  list(APPEND files src/config/config_impl_apple.mm)
  list(APPEND files src/core/rand_util_posix.cpp)
  list(APPEND files src/core/utils_mac.mm)
else()
  if (APPLE)
    list(APPEND files src/core/utils_mac.mm)
  endif()
  list(APPEND files src/core/rand_util_posix.cpp)
  list(APPEND files src/core/utils_linux.cpp)
  list(APPEND files src/core/utils_freebsd.cpp)
endif()

if (USE_ICU)
  list(APPEND files src/i18n/icu_util.cpp)
  list(APPEND hfiles src/i18n/icu_util.hpp)
endif()

add_library(yass_base STATIC
  ${files} ${hfiles}
  )

target_include_directories(yass_base PUBLIC
  ${SUPPORT_INCLUDE_DIRS})

target_include_directories(yass_base PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_include_directories(yass_base PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

target_compile_definitions(yass_base PUBLIC
  ${SUPPORT_DEFINITIONS})

target_link_libraries(yass_base PUBLIC ${SUPPORT_LIBS})

if (USE_LIBCXX)
  target_link_libraries(yass_base PUBLIC
    cxx ${libcxx_PUBLIC_LIBRARIES})
endif()

if (ANDROID)
  target_link_libraries(yass_base PUBLIC log)
endif()

add_library(yass_cli_lib OBJECT
  src/cli/cli_worker.cpp
  src/cli/cli_connection.cpp
  src/cli/cli_connection.hpp
  src/cli/cli_connection_stats.cpp
  src/cli/cli_connection_stats.hpp
  )
target_include_directories(yass_cli_lib PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(yass_cli_lib PUBLIC yass_base)

if (CLI)
  add_executable(yass_cli
    src/cli/cli.cpp
  )

  if (WIN32)
    set(CLI_MSVC_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/yass_cli.manifest")
    configure_file("src/core/win32.manifest" "${CLI_MSVC_MANIFEST}" @ONLY)
    target_sources(yass_cli PRIVATE ${CLI_MSVC_MANIFEST})
  endif()

  target_include_directories(yass_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

  target_link_libraries(yass_cli PUBLIC
    yass_cli_lib
    yass_base
    )

  if (NOT CMAKE_SKIP_INSTALL_RULES AND NOT WIN32 AND NOT IOS)
    install(TARGETS yass_cli RUNTIME DESTINATION bin)
    install(FILES doc/yass_cli.1 DESTINATION share/man/man1)
    # TODO add freebsd rc files
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID AND NOT OHOS)
      install(FILES yass.json.sample RENAME config.json DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/yass)
      install(FILES yass.service RENAME yass.service DESTINATION lib/systemd/system)

      install(FILES yass-redir.json.sample RENAME redir.json DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/yass)
      install(FILES yass-redir.service RENAME yass-redir.service DESTINATION lib/systemd/system)
    endif()
  endif()
endif()

add_library(yass_server_lib OBJECT
  src/server/server_connection.cpp
  src/server/server_connection.hpp
  )
target_include_directories(yass_server_lib PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(yass_server_lib PUBLIC yass_base)

if (SERVER)
  add_executable(yass_server
    src/server/server.cpp
    )

  if (WIN32)
    set(SERVER_MSVC_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/yass_server.manifest")
    configure_file("src/core/win32.manifest" "${SERVER_MSVC_MANIFEST}" @ONLY)
    target_sources(yass_server PRIVATE ${SERVER_MSVC_MANIFEST})
  endif()

  target_include_directories(yass_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

  target_link_libraries(yass_server PUBLIC
    yass_server_lib
    yass_base
    )

  if (NOT CMAKE_SKIP_INSTALL_RULES AND NOT WIN32 AND NOT IOS)
    install(TARGETS yass_server RUNTIME DESTINATION bin)
    install(FILES doc/yass_server.1 DESTINATION share/man/man1)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID AND NOT OHOS)
      install(FILES yass-server.json.sample RENAME server.json DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/yass)
      install(FILES yass-server.service RENAME yass-server.service DESTINATION lib/systemd/system)
    endif()
  endif()
endif()

set(APP_FEATURE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/feature.h")
configure_file("src/feature.h.in" "${APP_FEATURE_HEADER}" @ONLY)
message(STATUS "Enabled Feature: ${YASS_APP_FEATURES}")

# *****************************************************************************************
#           Source code: GUI part
# *****************************************************************************************

if (GUI)
    set(APP_NAME yass)
    set(SRC_FILES)
    if (GUI_FLAVOUR STREQUAL "android")
        list(APPEND SRC_FILES
          src/android/yass.cpp
          src/android/yass.hpp
          src/android/jni.cpp
          src/android/jni.hpp
          src/android/utils.hpp
          src/android/utils.cpp
        )
    elseif (GUI_FLAVOUR STREQUAL "windows")
        set(APP_MSVC_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/yass.manifest")
        configure_file("src/win32/yass.manifest.in" "${APP_MSVC_MANIFEST}" @ONLY)

        if (MINGW)
          set(APP_MINGW_MANIFEST_RC "${CMAKE_CURRENT_BINARY_DIR}/yass_manifest.rc")
          file(WRITE "${APP_MINGW_MANIFEST_RC}"
            "#include <winuser.h>\n"
            "1 RT_MANIFEST yass.manifest\n"
          )
        endif()

        set(APP_MSVC_VERSION_RC "${CMAKE_CURRENT_BINARY_DIR}/yass_version.rc")
        configure_file("src/win32/yass_version.rc.in" "${APP_MSVC_VERSION_RC}" @ONLY)
        set(APP_MSVC_ABOUT_DIALOG_RC "${CMAKE_CURRENT_BINARY_DIR}/yass_about.rc")
        configure_file("src/win32/yass_about.rc.in" "${APP_MSVC_ABOUT_DIALOG_RC}" @ONLY)

        list(APPEND SRC_FILES
          ${APP_MSVC_MANIFEST}
          ${APP_MINGW_MANIFEST_RC}
          ${APP_MSVC_VERSION_RC}
          ${APP_MSVC_ABOUT_DIALOG_RC}
          src/win32/yass.rc
          src/win32/yass.cpp
          src/win32/yass_frame.cpp
          src/win32/utils_win.cpp)
    elseif (GUI_FLAVOUR STREQUAL "cocoa")
        set(MAIN_STORYBOARD ${CMAKE_CURRENT_SOURCE_DIR}/src/mac/Base.lproj/Main.storyboard)
        set(ALLRESOURCES src/mac/yass.icns)
        if (${CMAKE_GENERATOR} MATCHES "^Xcode.*")
          set(ALLRESOURCES
            ${ALLRESOURCES}
            ${MAIN_STORYBOARD}
            src/mac/en.lproj/Localizable.strings
            src/mac/en.lproj/Main.strings
            src/mac/zh-Hans.lproj/Localizable.strings
            src/mac/zh-Hans.lproj/Main.strings
          )
        endif()
        set(ASSET_CATALOG src/mac/Assets.xcassets)
        set(ASSET_CATALOG_ASSETS Assets)
        list(APPEND SRC_FILES ${ALLRESOURCES})
        list(APPEND SRC_FILES
          ${ASSET_CATALOG})
        if (USE_ICU)
          list(APPEND SRC_FILES
            ${ICU_DATAFILE})
        endif()
        if (_CRASHPAD_BINARY)
          list(APPEND SRC_FILES
            ${_CRASHPAD_BINARY})
        endif()
        if (GUI_SANDBOX)
          list(APPEND SRC_FILES
            src/mac/yass.entitlements)
        endif()
        list(APPEND SRC_FILES
          src/mac/yass.icns)

        list(APPEND SRC_FILES
          src/mac/YassAppDelegate.mm
          src/mac/YassAppDelegate.h
          src/mac/YassViewController.mm
          src/mac/YassViewController.h
          src/mac/OptionViewController.mm
          src/mac/OptionViewController.h
          src/mac/utils.h
          src/mac/utils_mac.mm
          src/mac/main.mm)
    elseif (GUI_FLAVOUR STREQUAL "ios")
        set(MAIN_STORYBOARD ${CMAKE_CURRENT_SOURCE_DIR}/src/ios/Base.lproj/Main.storyboard)
        set(LAUNCH_STORYBOARD ${CMAKE_CURRENT_SOURCE_DIR}/src/ios/Base.lproj/LaunchScreen.storyboard)
        set(ALLRESOURCES src/ios/yass.icns)
        if (${CMAKE_GENERATOR} MATCHES "^Xcode.*")
          set(ALLRESOURCES
            ${ALLRESOURCES}
            ${MAIN_STORYBOARD}
            ${LAUNCH_STORYBOARD}
            src/ios/en.lproj/Localizable.strings
            src/ios/en.lproj/Main.strings
            src/ios/zh-Hans.lproj/Localizable.strings
            src/ios/zh-Hans.lproj/Main.strings)
        endif()
        set(ASSET_CATALOG src/ios/Assets.xcassets)
        set(ASSET_CATALOG_ASSETS Assets)
        list(APPEND SRC_FILES ${ALLRESOURCES})
        list(APPEND SRC_FILES
          ${ASSET_CATALOG})
        if (USE_ICU)
          list(APPEND SRC_FILES
            ${ICU_DATAFILE})
        endif()
        list(APPEND SRC_FILES
          src/ios/yass.icns)
        list(APPEND SRC_FILES
          src/ios/YassAppDelegate.mm
          src/ios/YassAppDelegate.h
          src/ios/YassViewController.mm
          src/ios/YassViewController.h
          src/ios/YassSceneDelegate.h
          src/ios/YassSceneDelegate.mm
          src/ios/utils.h
          src/ios/utils.mm
          src/ios/main.mm)
    elseif(GUI_FLAVOUR STREQUAL "gtk3")
        list(APPEND SRC_FILES
          src/gtk/yass.cpp
          src/gtk/yass_window.cpp
          src/gtk/option_dialog.cpp
          src/gtk/utils_gtk.cpp
        )
        include(CompilePo)
        create_po_target(yass en src/gtk/yass_en.po)
        create_po_target(yass zh_CN src/gtk/yass_zh_CN.po)
        list(APPEND SRC_FILES ${CMAKE_CURRENT_BINARY_DIR}/yass_en.gmo)
        list(APPEND SRC_FILES ${CMAKE_CURRENT_BINARY_DIR}/yass_zh_CN.gmo)
    elseif(GUI_FLAVOUR STREQUAL "gtk4")
        # Compile the gresource file with the glib-compile-resources.
        set(MAIN_GRESOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk4/yass.gresource.xml)
        set(MAIN_GRESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/resources.c)
        add_custom_command(OUTPUT ${MAIN_GRESOURCE_C}
          COMMAND ${GLIB_COMPILE_RESOURCES} ${MAIN_GRESOURCE} --target=${MAIN_GRESOURCE_C} --generate-source
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk4
          DEPENDS ${MAIN_GRESOURCE} src/gtk4/menu.ui src/gtk4/yass_window.ui src/gtk4/option_dialog.ui
          COMMENT "Compiling yass gresource")
        list(APPEND SRC_FILES
          ${MAIN_GRESOURCE_C}
          src/gtk4/yass.cpp
          src/gtk4/yass_window.cpp
          src/gtk4/option_dialog.cpp
          src/gtk/utils_gtk.cpp)
        # HOLDPLACE for TODO gettext support
        include(CompilePo)
        create_po_target(yass en src/gtk4/yass_en.po)
        create_po_target(yass zh_CN src/gtk4/yass_zh_CN.po)
        list(APPEND SRC_FILES ${CMAKE_CURRENT_BINARY_DIR}/yass_en.gmo)
        list(APPEND SRC_FILES ${CMAKE_CURRENT_BINARY_DIR}/yass_zh_CN.gmo)
    endif()

    # target_link_directories requires cmake 3.13
    link_directories(${GUI_LIBRARY_DIRS})

    if (GUI_FLAVOUR STREQUAL "ios" OR GUI_FLAVOUR STREQUAL "cocoa")
      set(GUI_OPTIONS MACOSX_BUNDLE)
    endif()

    if (GUI_FLAVOUR STREQUAL "android")
      add_library(${APP_NAME} SHARED ${GUI_OPTIONS}
          ${SRC_FILES}
          ${GUI_USE_FILE}
          )
    else()
      add_executable(${APP_NAME} WIN32 ${GUI_OPTIONS}
          ${SRC_FILES}
          ${GUI_USE_FILE}
          )
    endif()

    target_include_directories(${APP_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

    target_include_directories(${APP_NAME} SYSTEM PRIVATE
        ${GUI_INCLUDE_DIRS})

    if (GUI_FLAVOUR STREQUAL "android")
      target_include_directories(${APP_NAME} PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/android)
    elseif (GUI_FLAVOUR STREQUAL "windows")
      target_include_directories(${APP_NAME} PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/win32)
    elseif(GUI_FLAVOUR STREQUAL "cocoa")
      target_include_directories(${APP_NAME} PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/mac)
    elseif(GUI_FLAVOUR STREQUAL "ios")
      target_include_directories(${APP_NAME} PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/ios)
    elseif(GUI_FLAVOUR STREQUAL "gtk3")
      target_include_directories(${APP_NAME} PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk)
    elseif(GUI_FLAVOUR STREQUAL "gtk4")
      target_include_directories(${APP_NAME} PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk4)
    endif()

    foreach(GUI_DEFINITION IN LISTS GUI_DEFINITIONS)
        target_compile_definitions(${APP_NAME} PRIVATE ${GUI_DEFINITION})
    endforeach()

    target_compile_options(${APP_NAME} PRIVATE ${GUI_C_CXX_FLAGS})

    target_link_libraries(${APP_NAME} PUBLIC
      yass_cli_lib
      ${GUI_LIBRARIES}
      yass_base
    )

    if (GUI_FLAVOUR STREQUAL "windows" AND MSVC)
      # Most of the dlls are simply not required at startup (or at all, depending
      # on how the browser is used). The following dlls are interconnected and need to
      # be delayloaded together to ensure user32 does not load too early or at all,
      # depending on the process type: user32, gdi32, comctl32, comdlg32, cryptui,
      # d3d9, dwmapi, imm32, msi, ole32, oleacc, rstrtmgr, shell32, shlwapi, and
      # uxtheme.
      set (DELAY_LDFLAGS
        "/DELAYLOAD:api-ms-win-core-winrt-error-l1-1-0.dll"
        "/DELAYLOAD:api-ms-win-core-winrt-l1-1-0.dll"
        "/DELAYLOAD:api-ms-win-core-winrt-string-l1-1-0.dll"
        "/DELAYLOAD:comctl32.dll"
        "/DELAYLOAD:comdlg32.dll"
        "/DELAYLOAD:credui.dll"
        "/DELAYLOAD:cryptui.dll"
        "/DELAYLOAD:d3d11.dll"
        "/DELAYLOAD:d3d9.dll"
        "/DELAYLOAD:dwmapi.dll"
        "/DELAYLOAD:dxgi.dll"
        "/DELAYLOAD:dxva2.dll"
        "/DELAYLOAD:esent.dll"
        "/DELAYLOAD:gdi32.dll"
        "/DELAYLOAD:hid.dll"
        "/DELAYLOAD:imagehlp.dll"
        "/DELAYLOAD:imm32.dll"
        "/DELAYLOAD:msi.dll"
        "/DELAYLOAD:netapi32.dll"
        "/DELAYLOAD:ncrypt.dll"
        "/DELAYLOAD:ole32.dll"
        "/DELAYLOAD:oleacc.dll"
        "/DELAYLOAD:propsys.dll"
        "/DELAYLOAD:psapi.dll"
        "/DELAYLOAD:rpcrt4.dll"
        "/DELAYLOAD:rstrtmgr.dll"
        "/DELAYLOAD:setupapi.dll"
        "/DELAYLOAD:shell32.dll"
        "/DELAYLOAD:shlwapi.dll"
        "/DELAYLOAD:urlmon.dll"
        "/DELAYLOAD:user32.dll"
        "/DELAYLOAD:usp10.dll"
        "/DELAYLOAD:uxtheme.dll"
        "/DELAYLOAD:wer.dll"
        "/DELAYLOAD:wevtapi.dll"
        "/DELAYLOAD:wininet.dll"
        "/DELAYLOAD:winusb.dll"
        "/DELAYLOAD:wsock32.dll"
        "/DELAYLOAD:wtsapi32.dll"

        "/DELAYLOAD:advapi32.dll"
        "/DELAYLOAD:crypt32.dll"
        "/DELAYLOAD:dbghelp.dll"
        "/DELAYLOAD:dhcpcsvc.dll"
        "/DELAYLOAD:dwrite.dll"
        "/DELAYLOAD:iphlpapi.dll"
        "/DELAYLOAD:oleaut32.dll"
        "/DELAYLOAD:secur32.dll"
        "/DELAYLOAD:uiautomationcore.dll"
        "/DELAYLOAD:userenv.dll"
        "/DELAYLOAD:winhttp.dll"
        "/DELAYLOAD:winmm.dll"
        "/DELAYLOAD:winspool.drv"
        "/DELAYLOAD:wintrust.dll"
        "/DELAYLOAD:ws2_32.dll"
        )
      # TODO implement own delay load helper
      # https://docs.microsoft.com/en-us/cpp/build/reference/understanding-the-helper-function?view=msvc-170
      # __pfnDliFailureHook2: Delay load failure hook that generates a crash report. By default a failure
      # to delay load will trigger an exception handled by the delay load runtime and
      # this won't generate a crash report.
      # https://docs.microsoft.com/en-us/cpp/build/reference/failure-hooks?view=vs-2019
      # __pfnDliNotifyHook2: verify if loaded libraries are allowed
      # FIXME it seems delay loader's delayimp library isn't be found correctly
      # when it comes with static CRT build.
      string(REPLACE ";" " " DELAY_LDFLAGS "${DELAY_LDFLAGS}")
      set_target_properties(${APP_NAME} PROPERTIES
        LINK_FLAGS "${DELAY_LDFLAGS}")

      # enforce default delayload implementation
      target_link_libraries(${APP_NAME} PUBLIC delayimp)
    endif()

    if (GUI_FLAVOUR STREQUAL "windows" AND MSVC)
        set_target_properties(${APP_NAME} PROPERTIES
            COMPILE_PDB_NAME ${APP_NAME}
            COMPILE_PDB_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
        )

        # CMake currently doesn't model the Windows application entry point for Unicode.
        # It works in the Visual Studio generator only because this code and this code
        # recognizes the _UNICODE definition and modifies the generated .vcxproj field
        # for the CharacterSet accordingly.
        # MSBuild turns this in to the proper link flag for /entry:wWinMainCRTStartup
        # (or something like that).
        # https://docs.microsoft.com/en-us/cpp/build/reference/entry-entry-point-symbol?view=msvc-170
        # https://gitlab.kitware.com/cmake/cmake/-/issues/21202
        get_target_property(_APP_LINK_FLAGS ${APP_NAME} LINK_FLAGS)
        if (NOT _APP_LINK_FLAGS)
          set(_APP_LINK_FLAGS "")
        endif()
        set_target_properties(${APP_NAME} PROPERTIES
            LINK_FLAGS "${_APP_LINK_FLAGS} /ENTRY:wWinMainCRTStartup")
    endif()

    # necessary for GCC
    if (GUI_FLAVOUR STREQUAL "windows" AND NOT MSVC)
      set_target_properties(${APP_NAME} PROPERTIES
        LINK_FLAGS "-municode")
    endif()

    if (GUI_FLAVOUR STREQUAL "cocoa")
        # Handle the Storyboard ourselves
        if(NOT ${CMAKE_GENERATOR} MATCHES "^Xcode.*")
          # Compile the storyboard file with the ibtool.
          add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${IBTOOL}
            --errors
            --warnings
            --notices
            --output-format human-readable-text
            --compile ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app/Contents/Resources/Base.lproj/Main.storyboardc
            ${MAIN_STORYBOARD}
            COMMENT "Compiling Main storyboard")

          set(YASS_LANGS)
          file(GLOB YASS_LPROJS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src/mac/ "${CMAKE_CURRENT_SOURCE_DIR}/src/mac/*.lproj")
          foreach(LPROJ ${YASS_LPROJS})
            string(REGEX REPLACE "^.*/([a-z]+)\\.lproj/.*$" "\\1" _LANG ${LPROJ})
            list(FIND YASS_LANGS "${_LANG}" _INDEX)
            if(_INDEX EQUAL -1 AND NOT _LANG STREQUAL "Base.lproj")
              list(APPEND YASS_LANGS ${_LANG})
              add_custom_command(TARGET ${APP_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app/Contents/Resources
                COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/src/mac/${_LANG} ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app/Contents/Resources/
              COMMENT "Copying language file: ${_LANG}")
            endif()
          endforeach()
        else()
          set_source_files_properties("${MAIN_STORYBOARD}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/Base.lproj")
          set_source_files_properties("src/mac/en.lproj/Localizable.strings" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
          set_source_files_properties("src/mac/en.lproj/Main.strings" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
          set_source_files_properties("src/mac/zh-Hans.lproj/Localizable.strings" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
          set_source_files_properties("src/mac/zh-Hans.lproj/Main.strings" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
        endif()

        # adds to Copy Bundle Resources build phase
        set_source_files_properties("src/mac/yass.icns" PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources")
        set_source_files_properties("${ASSET_CATALOG}" PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources")
        if (USE_ICU)
          set_source_files_properties("${ICU_DATAFILE}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
        endif()
        if (_CRASHPAD_BINARY)
          set_source_files_properties("${_CRASHPAD_BINARY}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
        endif()

        set(APP_MACOSX_BUNDLE_INFO "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
        configure_file("src/mac/Info.plist.in" "${APP_MACOSX_BUNDLE_INFO}" @ONLY)

        set_target_properties(${APP_NAME} PROPERTIES
            RESOURCE "${ALLRESOURCES}"
            MACOSX_BUNDLE_ICON_FILE yass.icns
            MACOSX_BUNDLE_COPYRIGHT "${YASS_APP_COPYRIGHT}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "it.gui.yass"
            MACOSX_BUNDLE_BUNDLE_NAME ${PACKAGE_NAME}
            MACOSX_BUNDLE_INFO_PLIST ${APP_MACOSX_BUNDLE_INFO}
            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
            XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
            XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES"
            XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
            #XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME "YES"
            #XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS "NO"
            #XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME ${ASSET_CATALOG_ASSETS}
          )
        if (GUI_SANDBOX)
          set_target_properties(${APP_NAME} PROPERTIES
              XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_SOURCE_DIR}/src/mac/yass.entitlements
          )
        endif()
    elseif(GUI_FLAVOUR STREQUAL "ios")
        if(NOT ${CMAKE_GENERATOR} MATCHES "^Xcode.*")
          # Compile the storyboard file with the ibtool.
          add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${IBTOOL}
            --errors
            --warnings
            --notices
            --output-format human-readable-text
            --compile ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app/Base.lproj/Main.storyboardc
            ${MAIN_STORYBOARD}
            COMMENT "Compiling Main storyboard")
          add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${IBTOOL}
            --errors
            --warnings
            --notices
            --output-format human-readable-text
            --compile ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app/Base.lproj/LaunchScreen.storyboardc
            ${LAUNCH_STORYBOARD}
            COMMENT "Compiling LaunchScreen storyboard")

          set(YASS_LANGS)
          file(GLOB YASS_LPROJS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src/ios/ "${CMAKE_CURRENT_SOURCE_DIR}/src/ios/*.lproj")
          foreach(LPROJ ${YASS_LPROJS})
            string(REGEX REPLACE "^.*/([a-z]+)\\.lproj/.*$" "\\1" _LANG ${LPROJ})
            list(FIND YASS_LANGS "${_LANG}" _INDEX)
            if(_INDEX EQUAL -1 AND NOT _LANG STREQUAL "Base.lproj")
              list(APPEND YASS_LANGS ${_LANG})
              add_custom_command(TARGET ${APP_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app
                COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/src/ios/${_LANG} ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app
              COMMENT "Copying language file: ${_LANG}")
            endif()
          endforeach()
        else()
          set_source_files_properties("${MAIN_STORYBOARD}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/Base.lproj")
          set_source_files_properties("${LAUNCH_STORYBOARD}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/Base.lproj")
          set_source_files_properties("src/ios/en.lproj/Localizable.strings" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
          set_source_files_properties("src/ios/en.lproj/Main.strings" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
          set_source_files_properties("src/ios/zh-Hans.lproj/Localizable.strings" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
          set_source_files_properties("src/ios/zh-Hans.lproj/Main.strings" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
        endif()

        # For iOS Apple uses a flat bundle layout where no Resources folder exist.
        # Therefore CMake strips the Resources folder name from the specified location.
        set_source_files_properties("src/ios/yass.icns" PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources")
        set_source_files_properties("${ASSET_CATALOG}" PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources")
        if (USE_ICU)
          set_source_files_properties("${ICU_DATAFILE}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
        endif()

        # Fixme
        set(APP_MACOSX_BUNDLE_INFO "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
        configure_file("src/ios/Info.plist.in" "${APP_MACOSX_BUNDLE_INFO}" @ONLY)

        set_target_properties(${APP_NAME} PROPERTIES
          RESOURCE "${ALLRESOURCES}"
          OUTPUT_NAME "${APP_NAME}"
          MACOSX_BUNDLE ON
          MACOSX_BUNDLE_INFO_PLIST ${APP_MACOSX_BUNDLE_INFO}
          MACOSX_BUNDLE_BUNDLE_NAME ${PACKAGE_FULL_NAME}
          MACOSX_BUNDLE_BUNDLE_VERSION ${PACKAGE_VERSION}
          MACOSX_BUNDLE_COPYRIGHT "${YASS_APP_COPYRIGHT}"
          MACOSX_BUNDLE_GUI_IDENTIFIER "it.gui.ios.yass"
          MACOSX_BUNDLE_INFO_STRING "${APP_NAME}"
          MACOSX_BUNDLE_LONG_VERSION_STRING "${YASS_APP_TAG}-${YASS_APP_SUBTAG}"
          MACOSX_BUNDLE_SHORT_VERSION_STRING "${YASS_APP_TAG}"
          MACOSX_BUNDLE_ICON_FILE "AppIcon"
          XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "it.gui.ios.yass"
          XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_SOURCE_DIR}/src/ios/yass.entitlements
          XCODE_ATTRIBUTE_MARKETING_VERSION "${YASS_APP_VERSION}"
          XCODE_ATTRIBUTE_PRODUCT_NAME "${PACKAGE_NAME}"
          XCODE_GENERATE_SCHEME "TRUE"
          XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
          # Workaround for https://gitlab.kitware.com/cmake/cmake/-/issues/15183
          # Apps must have install step enabled
          XCODE_ATTRIBUTE_INSTALL_PATH  "$(LOCAL_APPS_DIR)"
          XCODE_ATTRIBUTE_SKIP_INSTALL  NO
          XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
          XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
          XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
          XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
          XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES"
          #XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME "YES"
        )
        if ((NOT XCODE_CODESIGN_IDENTITY STREQUAL "-") OR XCODE_DEPLOYMENT_TEAM)
          set_target_properties(${APP_NAME} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${XCODE_CODESIGN_IDENTITY}"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${XCODE_DEPLOYMENT_TEAM}"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
        endif()

        # =======================
        # YassPacketTunnel extensions
        # =======================
        find_library(NetworkExtension_LIBRARY NetworkExtension REQUIRED)

        set(PLUGIN_SRC
          src/ios/tun2proxy.h
          src/ios/tun2proxy.mm
          src/ios/extensions/PacketTunnelProvider.h
          src/ios/extensions/PacketTunnelProvider.mm
          src/ios/extensions/PacketTunnel.entitlements
          )

        add_executable(YassPacketTunnel MACOSX_BUNDLE ${PLUGIN_SRC})

        target_include_directories(YassPacketTunnel PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/src/ios/extensions)
        target_compile_options(YassPacketTunnel PRIVATE -fapplication-extension -fmodules -gmodules -fobjc-arc -fobjc-weak)
        target_link_libraries(YassPacketTunnel PRIVATE ${NetworkExtension_LIBRARY})
        set_target_properties(YassPacketTunnel PROPERTIES LINK_OPTIONS -fapplication-extension -e _NSExtensionMain -fobjc-arc -fobjc-link-runtime)

        set_target_properties(YassPacketTunnel PROPERTIES
          OUTPUT_NAME "YassPacketTunnel"
          XCODE_PRODUCT_TYPE com.apple.product-type.app-extension
          BUNDLE_EXTENSION appex
          MACOSX_BUNDLE ON
          MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/src/ios/extensions/Info.plist
          MACOSX_BUNDLE_BUNDLE_NAME "YassPacketTunnel"
          MACOSX_BUNDLE_BUNDLE_VERSION ${PACKAGE_VERSION}
          MACOSX_BUNDLE_COPYRIGHT "${YASS_APP_COPYRIGHT}"
          MACOSX_BUNDLE_GUI_IDENTIFIER "it.gui.ios.yass.PacketTunnel"
          MACOSX_BUNDLE_INFO_STRING "YassPacketTunnel"
          MACOSX_BUNDLE_LONG_VERSION_STRING "${YASS_APP_TAG}-${YASS_APP_SUBTAG}"
          MACOSX_BUNDLE_SHORT_VERSION_STRING "${YASS_APP_TAG}"
          XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "it.gui.ios.yass.PacketTunnel"
          XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_SOURCE_DIR}/src/ios/extensions/PacketTunnel.entitlements
          XCODE_ATTRIBUTE_MARKETING_VERSION "${YASS_APP_VERSION}"
          XCODE_ATTRIBUTE_PRODUCT_NAME "YassPacketTunnel"
          XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
          XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES YES
          XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
          XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES
          XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER ""
          XCODE_ATTRIBUTE_APPLICATION_EXTENSION_API_ONLY "YES"
          XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/../../Frameworks"
          )

        if ((NOT XCODE_CODESIGN_IDENTITY STREQUAL "-") OR XCODE_DEPLOYMENT_TEAM)
          set_target_properties(YassPacketTunnel PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${XCODE_CODESIGN_IDENTITY}"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${XCODE_DEPLOYMENT_TEAM}"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
        endif()

        add_dependencies(${APP_NAME} YassPacketTunnel)
        set_target_properties(${APP_NAME} PROPERTIES
          XCODE_EMBED_APP_EXTENSIONS YassPacketTunnel
          XCODE_EMBED_APP_EXTENSIONS_REMOVE_HEADERS_ON_COPY "YES"
          XCODE_EMBED_APP_EXTENSIONS_CODE_SIGN_ON_COPY "YES"
        )

        target_link_libraries(${APP_NAME} PRIVATE ${NetworkExtension_LIBRARY})
    endif()

    if (NOT CMAKE_SKIP_INSTALL_RULES)
      # installable, except on macOS when marked as MACOSX_BUNDLE
      if (NOT APPLE)
        install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)
      endif()

      if (GUI_FLAVOUR STREQUAL "gtk3" OR GUI_FLAVOUR STREQUAL "gtk4")
        install(FILES
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/yass.desktop
          DESTINATION share/applications)

        install(FILES
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/yass.png
          DESTINATION share/pixmaps)

        install(DIRECTORY
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/16x16
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/22x22
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/24x24
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/32x32
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/48x48
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/128x128
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/256x256
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/512x512
          DESTINATION share/icons/hicolor)
      endif()
    endif()
endif()

# *****************************************************************************************
#           Source code: LICENSE part
# *****************************************************************************************

if(NOT CMAKE_SKIP_INSTALL_RULES)
  file(READ LICENSE                                         _YASS_LICENSE)
  file(READ third_party/abseil-cpp/LICENSE                  _ABSEIL_CPP_LICENSE)
  file(READ third_party/asio/asio/LICENSE_1_0.txt           _ASIO_LICENSE)
  file(READ third_party/boringssl/src/LICENSE               _BORINGSSL_LICENSE)
  file(READ third_party/googleurl/LICENSE                   _GOOGLEURL_LICENSE)
  file(READ third_party/icu/LICENSE                         _ICU_LICENSE)
  file(READ third_party/json/LICENSE.MIT                    _JSON_LICENSE)
  file(READ third_party/leveldb/LICENSE                     _LEVELDB_LICENSE)
  file(READ third_party/libc++/trunk/LICENSE.TXT            _LIBCXX_LICENSE)
  file(READ third_party/libc++abi/trunk/LICENSE.TXT         _LIBCXXABI_LICENSE)
  file(READ third_party/libunwind/trunk/LICENSE.TXT         _LIBUNWIND_LICENSE)
  file(READ third_party/lss/LICENSE                         _LSS_LICENSE)
  file(READ third_party/mbedtls/LICENSE                     _MBEDTLS_LICENSE)
  file(READ third_party/protobuf/LICENSE                    _PROTOBUF_LICENSE)
  file(READ third_party/quiche/src/LICENSE                  _QUICHE_LICENSE)
  file(READ third_party/re2/LICENSE                         _RE2_LICENSE)
  file(READ third_party/snappy/src/COPYING                  _SNAPPY_LICENSE)
  file(READ third_party/sqlite/LICENSE                      _SQLITE_LICENSE)
  file(READ third_party/tcmalloc/src/LICENSE                _TCMALLOC_LICENSE)
  file(READ third_party/tun2proxy/LICENSE                   _TUN2PROXY_LICENSE)
  file(READ third_party/zlib/LICENSE                        _ZLIB_LICENSE)

  set(LICENSE_FILE "${CMAKE_CURRENT_BINARY_DIR}/LICENSE")
  configure_file("LICENSE.tmpl" "${LICENSE_FILE}" @ONLY)
  install(FILES "${LICENSE_FILE}" DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

# *****************************************************************************************
#           Source code: Unit Tests part
# *****************************************************************************************

if (BUILD_TESTS)
  set(yass_test_SOURCE
    src/test_util.cpp
    src/network_test.cpp
    src/ss_test.cpp
    src/compiler_test.cpp
    src/config/config_test.cpp
    src/core/cipher_test.cpp
    src/core/c-ares_test.cpp
    src/core/process_utils_test.cpp
    src/core/utils_test.cpp
    )

  if (IOS)
    enable_testing()
    find_package(XCTest REQUIRED)

    xctest_add_bundle(yass_test ${APP_NAME}
      src/ss_test.mm
      ${yass_test_SOURCE})

    xctest_add_test(XCTest.yass_test yass_test)

    if ((NOT XCODE_CODESIGN_IDENTITY STREQUAL "-") OR XCODE_DEPLOYMENT_TEAM)
      set_target_properties(yass_test PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${XCODE_CODESIGN_IDENTITY}"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${XCODE_DEPLOYMENT_TEAM}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
    endif()
  else()
    add_executable(yass_test ${yass_test_SOURCE})
  endif()
  if (USE_LEVELDB)
    target_sources(yass_test PRIVATE third_party/leveldb_test.cc)
  endif()
  if (USE_SQLITE)
    target_sources(yass_test PRIVATE third_party/sqlite/sqlite3_test.cc)
  endif()
  target_include_directories(yass_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
      ${CMAKE_CURRENT_SOURCE_DIR}/src/server
      )
  target_link_libraries(yass_test PUBLIC
      yass_cli_lib
      yass_server_lib
      yass_gtest
      )
  if (WIN32)
    set(TEST_MSVC_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/yass_test.manifest")
    configure_file("src/core/win32.manifest" "${TEST_MSVC_MANIFEST}" @ONLY)
    target_sources(yass_test PRIVATE ${TEST_MSVC_MANIFEST})
  endif()
  add_custom_target(check
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/yass_test
    DEPENDS yass_test
    COMMENT "yass unittests"
    USES_TERMINAL
  )

  if (USE_CURL)
    if (MSVC AND MSVC_CRT_LINKAGE STREQUAL "dynamic")
      if("${MSVC_PROCESSOR_ARCHITECTURE}" STREQUAL "arm64")
        set(CURL_MSVC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libcurl-vc16-arm64-release-static-ipv6-sspi-schannel")
      elseif("${MSVC_PROCESSOR_ARCHITECTURE}" STREQUAL "amd64")
        set(CURL_MSVC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libcurl-vc16-x64-release-static-ipv6-sspi-schannel")
      elseif("${MSVC_PROCESSOR_ARCHITECTURE}" STREQUAL "x86")
        set(CURL_MSVC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libcurl-vc16-x86-release-static-ipv6-sspi-schannel")
      endif()
      if (EXISTS ${CURL_MSVC_DIR}/include/curl/curl.h)
        set(MSVC_CURL_EXIST TRUE)
      endif()
      if (MSVC_CURL_EXIST)
        message(STATUS "Using existing curl binary: ${CURL_MSVC_DIR}")
        add_library(libcurl_a STATIC IMPORTED)
        set_property(TARGET libcurl_a PROPERTY
                     IMPORTED_LOCATION "${CURL_MSVC_DIR}/lib/libcurl_a.lib")
        set(CURL_FOUND TRUE)
        set(CURL_CFLAGS /DCURL_STATICLIB)
        set(CURL_INCLUDE_DIRS ${CURL_MSVC_DIR}/include)
        set(CURL_LIBRARY_DIRS ${CURL_MSVC_DIR}/lib)
        # The static library name has an _a suffix in the basename
        set(CURL_LIBRARIES libcurl_a normaliz Wldap32)
      endif()
    endif()
    if (MINGW)
      if (OS_AARCH64)
        set(CURL_MINGW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/curl-8.4.0_7-win64a-mingw")
      elseif(OS_X86)
        set(CURL_MINGW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/curl-8.4.0_7-win32-mingw")
      else()
        set(CURL_MINGW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/curl-8.4.0_7-win64-mingw")
      endif()
      if (EXISTS ${CURL_MINGW_DIR}/include/curl/curl.h)
        set(MINGW_CURL_EXIST TRUE)
      endif()
      if (MINGW_CURL_EXIST)
        message(STATUS "Using existing curl binary: ${CURL_MINGW_DIR}")
        add_library(curl STATIC IMPORTED)
        set_property(TARGET curl PROPERTY
                     IMPORTED_LOCATION "${CURL_MINGW_DIR}/lib/libcurl.dll.a")
        set(CURL_FOUND TRUE)
        #set(CURL_CFLAGS -DCURL_STATICLIB)
        set(CURL_INCLUDE_DIRS ${CURL_MINGW_DIR}/include)
        set(CURL_LIBRARY_DIRS ${CURL_MINGW_DIR}/lib)
        set(CURL_LIBRARIES curl)
      endif()
    endif()
    if (NOT MSVC_CURL_EXIST AND NOT MINGW_CURL_EXIST)
      find_package(PkgConfig)
      if (PKG_CONFIG_FOUND)
        pkg_check_modules(CURL libcurl)
      endif()
    endif()
  endif()
  if (USE_CURL AND CURL_FOUND)
    target_compile_definitions(yass_test PRIVATE HAVE_CURL)
    target_include_directories(yass_test SYSTEM PRIVATE ${CURL_INCLUDE_DIRS})
    target_compile_options(yass_test PRIVATE ${CURL_CFLAGS} ${CURL_CFLAGS_OTHER})
    target_link_directories(yass_test PRIVATE ${CURL_LIBRARY_DIRS})
    target_link_libraries(yass_test PRIVATE ${CURL_LIBRARIES})
    target_link_options(yass_test PRIVATE ${CURL_LDFLAGS} ${CURL_LDFLAGS_OTHER})
  else()
    set(USE_CURL OFF)
  endif()
endif()

if (BUILD_BENCHMARKS)
  set(yass_benchmark_SOURCE
    src/ss_benchmark.cpp
    )
  if (IOS)
    enable_testing()
    find_package(XCTest REQUIRED)

    xctest_add_bundle(yass_benchmark ${APP_NAME}
      src/ss_benchmark.mm
      ${yass_benchmark_SOURCE})

    xctest_add_test(XCTest.yass_benchmark yass_benchmark)

    if ((NOT XCODE_CODESIGN_IDENTITY STREQUAL "-") OR XCODE_DEPLOYMENT_TEAM)
      set_target_properties(yass_benchmark PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${XCODE_CODESIGN_IDENTITY}"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${XCODE_DEPLOYMENT_TEAM}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
    endif()
  else()
    add_executable(yass_benchmark
      ${yass_benchmark_SOURCE})
  endif()
  target_include_directories(yass_benchmark PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
      ${CMAKE_CURRENT_SOURCE_DIR}/src/server
      )
  target_link_libraries(yass_benchmark PUBLIC
      yass_cli_lib
      yass_server_lib
      benchmark::benchmark
      )
  if (WIN32)
    set(BENCHMARK_MSVC_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/yass_benchmark.manifest")
    configure_file("src/core/win32.manifest" "${BENCHMARK_MSVC_MANIFEST}" @ONLY)
    target_sources(yass_benchmark PRIVATE ${BENCHMARK_MSVC_MANIFEST})
  endif()
endif()

# Configure CPack.
if(NOT DEFINED CPACK_PACKAGE_INSTALL_DIRECTORY)
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "yass")
endif()
if(NOT DEFINED CPACK_PACKAGE_VENDOR)
  set(CPACK_PACKAGE_VENDOR "yass")
endif()
set(CPACK_PACKAGE_VERSION_MAJOR ${PACKAGE_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PACKAGE_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PACKAGE_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PACKAGE_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/GPL-2.0.rtf")
if(WIN32 AND NOT UNIX)
  set(CPACK_NSIS_COMPRESSOR "/SOLID lzma \r\n SetCompressorDictSize 32")
  if(NOT DEFINED CPACK_PACKAGE_INSTALL_REGISTRY_KEY)
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "yass")
  endif()
  #set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\cmake\\\\nsis_logo.bmp")
  set(CPACK_NSIS_PACKAGE_NAME "Yet Another Shadow Socket")
  set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\src\\\\win32\\\\yass.ico")
  set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\src\\\\win32\\\\yass.ico")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL "ON")
  if( CMAKE_CL_64 )
    if(NOT DEFINED CPACK_NSIS_INSTALL_ROOT)
      set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    endif()
  endif()
  set(CPACK_NSIS_CREATE_ICONS_EXTRA
      "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\yass.lnk' '$INSTDIR\\\\bin\\\\yass.exe'"
  )
  set(CPACK_NSIS_DELETE_ICONS_EXTRA
      "Delete '$SMPROGRAMS\\\\$START_MENU\\\\yass.lnk'"
  )
endif()
include(CPack)
