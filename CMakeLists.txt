cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

if (CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(FATAL_ERROR "In source-tree build not support")
endif()

set(PACKAGE_NAME            "yass")
set(PACKAGE_FULL_NAME       "Yet Another Shadow Socket")
set(PACKAGE_VERSION_MAJOR   1)
set(PACKAGE_VERSION_MINOR   0)
set(PACKAGE_VERSION_PATCH   0)
set(PACKAGE_VERSION_TWEAK   0)
set(PACKAGE_VERSION ${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_PATCH})

# Compiler id for Apple Clang is now AppleClang.
# https://cmake.org/cmake/help/latest/policy/CMP0025.html
if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif (POLICY CMP0025)

# Project version variables are the empty string if version is unspecified
if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

# Select the MSVC runtime library for use by compilers targeting the MSVC ABI.
# Use CMAKE_MSVC_CRT_LINKAGE variable internally, option: dynamic and static
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
set(CMAKE_MSVC_CRT_LINKAGE "dynamic" CACHE STRING "")
if (VCPKG_CRT_LINKAGE AND NOT VCPKG_CRT_LINKAGE STREQUAL CMAKE_MSVC_CRT_LINKAGE)
  message(FATAL_ERROR "CRT LINKAGE not matched in configuration")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:${CMAKE_MSVC_CRT_LINKAGE},dynamic>:DLL>" CACHE STRING "")

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
set(YASS_APP_COPYRIGHT "Copyright 2022 Chilledheart. All rights reserved.")

# update LAST_CHANGE file
# format d740da257583289dbebd2eb37e8668928fac5ead-refs/branch-heads/4692{32}
find_program(GIT_EXECUTABLE NAMES git git.exe git.cmd)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
  execute_process(COMMAND
    ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE LAST_CHANGE_REF
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND
    ${GIT_EXECUTABLE} describe --abbrev=0 --tags HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE LATEST_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

  if (LATEST_TAG)
    execute_process(COMMAND
      ${GIT_EXECUTABLE} rev-list ${LATEST_TAG}..HEAD --count
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      OUTPUT_VARIABLE COUNT_FROM_LATEST_TAG
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    set(ABBREV_REF "${LATEST_TAG}\{${COUNT_FROM_LATEST_TAG}\}")
  else()
    execute_process(COMMAND
      ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      OUTPUT_VARIABLE ABBREV_REF
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()

  execute_process(COMMAND
    ${GIT_EXECUTABLE} describe --abbrev=40 --tags HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE YASS_APP_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

  set(YASS_APP_LAST_CHANGE "${LAST_CHANGE_REF}-refs/branch-heads/${ABBREV_REF}")
  if (NOT YASS_APP_TAG)
    set(YASS_APP_TAG "${YASS_APP_LAST_CHANGE}")
  endif()
else()
  # read if running from tarball
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/LAST_CHANGE YASS_APP_LAST_CHANGE)
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/TAG YASS_APP_TAG)
endif()

set(APP_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/version.h")
configure_file("src/version.h.in" "${APP_VERSION_HEADER}" @ONLY)
# for version.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})

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

if (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10")
endif()

enable_language(C)
enable_language(CXX)

## Setup compiler informations
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(COMPILER_CLANG TRUE)
else()
  set(COMPILER_CLANG FALSE)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(COMPILER_GCC TRUE)
else()
  set(COMPILER_GCC FALSE)
endif()

if (MSVC AND NOT COMPILER_CLANG)
  set(COMPILER_MSVC TRUE)
else()
  set(COMPILER_MSVC FALSE)
endif()

if((${CMAKE_LINKER} MATCHES "lld") AND (${CMAKE_AR} MATCHES "llvm-lib"))
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

if (MSVC)
  message(STATUS "Build with MSVC target architecture: ${MSVC_PROCESSOR_ARCHITECTURE}")
  message(STATUS "Build with MSVC ${CMAKE_MSVC_CRT_LINKAGE} CRT library")
elseif(APPLE)
  message(STATUS "Build with target architecture: ${CMAKE_OSX_ARCHITECTURES}")
else()
  message(STATUS "Build with target architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

## Export build compilation database if possible
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

## Enforce C/C++ standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)

if (MSVC AND NOT COMPILER_CLANG)
  set(CMAKE_CXX_STANDARD 17)
endif()

option(CLI "Build with cli." ON)
option(SERVER "Build with server." ON)
option(GUI "Build against GUI." OFF)
option(GUI_SANDBOX "Build against GUI sandbox (MAC Only)." OFF)
option(BUILD_TESTS "Build with test." OFF)
option(OPTIMIZED_PROTOC "Force protobuf compiler to be built with optimization" OFF)

include(config-ix)

if (CMAKE_CROSSCOMPILING OR (OPTIMIZED_PROTOC AND CMAKE_CONFIGURATION_TYPES))
  set(USE_HOST_TOOLS ON)
endif()

# when crosscompiling import the executable targets from a file
if (USE_HOST_TOOLS)
  include(CrossCompile)
  create_cross_target(yass NATIVE "" Release)
endif()
if(YASS_TARGET_IS_CROSSCOMPILE_HOST)
# Dummy use to avoid CMake Warning: Manually-specified variables were not used
# (this is a variable that CrossCompile sets on recursive invocations)
endif()

if (NOT MSVC AND NOT APPLE)
  include(GNUInstallDirs)
endif()

# *****************************************************************************************
#           Platform-specific
# *****************************************************************************************

if (MSVC)
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
    add_definitions(-D_USING_V110_SDK71_)
    add_definitions(-D_WIN32_WINNT=0x0501)
    add_definitions(-DWINVER=0x0501)
    # for winapifamily.h
    include_directories(third_party/xp-compatible SYSTEM)
  else()
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-DWINVER=0x0601)
  endif()

  # set linker option for x86
  if (MSVC_PROCESSOR_ARCHITECTURE STREQUAL "x86")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH /largeaddressaware")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH /largeaddressaware")
  endif()

  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DYNAMICBASE /NXCOMPAT")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DYNAMICBASE /NXCOMPAT")

  if (ALLOW_XP)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Zc:threadSafeInit-")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:threadSafeInit-")
  endif()

  # set linker for subsystem
  # Windows ARM64 requires Windows 10
  if (MSVC_PROCESSOR_ARCHITECTURE STREQUAL "arm64")
    set(SUBSYSTEM_VERSION_SUFFIX ",10.0")
  elseif (MSVC_PROCESSOR_ARCHITECTURE STREQUAL "amd64")
    # 5.02 = Windows Server 2003/XP SP2
    set(SUBSYSTEM_VERSION_SUFFIX ",5.02")
  elseif (MSVC_PROCESSOR_ARCHITECTURE STREQUAL "x86")
    # 5.01 = Windows XP
    set(SUBSYSTEM_VERSION_SUFFIX ",5.01")
  endif()
  set(CMAKE_C_CREATE_WIN32_EXE "/subsystem:windows${SUBSYSTEM_VERSION_SUFFIX}")
  set(CMAKE_CXX_CREATE_WIN32_EXE "/subsystem:windows${SUBSYSTEM_VERSION_SUFFIX}")
  set(CMAKE_C_CREATE_CONSOLE_EXE "/subsystem:console${SUBSYSTEM_VERSION_SUFFIX}")
  set(CMAKE_CXX_CREATE_CONSOLE_EXE "/subsystem:console${SUBSYSTEM_VERSION_SUFFIX}")
endif()

# *****************************************************************************************
#           Third Party Libraries
# *****************************************************************************************

include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

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

if (MSVC)
    option(VCPKG_ROOT_DIR "" "vcpkg root path")
    set(VCPKG_INCLUDE_DIRS
      ${VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/include)
    set(VCPKG_BIN_PATH ${VCPKG_ROOT_DIR}/installed/x86-windows/bin)
    set(VCPKG_STATIC_LIB_PATH ${VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/lib)
    set(VCPKG_STATIC_LIB_DEBUG_PATH ${VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/debug/lib)

    include_directories(${VCPKG_INCLUDE_DIRS} SYSTEM)
    set(CMAKE_REQUIRED_INCLUDES
        ${VCPKG_INCLUDE_DIRS}
        ${CMAKE_REQUIRED_INCLUDES})

    set(SYSTEM_LIBS ${SYSTEM_LIBS} ShLwApi WS2_32 WinMM Gdi32 ComDlg32 shell32 Ole32 WinSpool)

    if (CMAKE_MSVC_CRT_LINKAGE STREQUAL "static")
      # replace /MD with /MT
      set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
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
    if (COMPILER_CLANG)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Y-")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Y-")
    endif()

    # enforce default delayload implementation
    set(SYSTEM_LIBS ${SYSTEM_LIBS} delayimp)

    # this must come at last
    if (CMAKE_MSVC_CRT_LINKAGE STREQUAL "static")
      set(SYSTEM_LIBS ${SYSTEM_LIBS} uafxcw$<$<CONFIG:Debug>:d> libcmt$<$<CONFIG:Debug>:d>)
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
    if (UNIX AND NOT APPLE)
      include_directories(third_party/lss)
    endif()
    if (APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -weak_framework AppKit")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -weak_framework AppKit")
    endif()
endif()

set(SUPPORT_LIBS ${SUPPORT_LIBS} ${SYSTEM_LIBS})

if (MSVC AND GUI)
  message(STATUS "Build with MFC unicode ${CMAKE_MSVC_CRT_LINKAGE} library")
  if (CMAKE_MSVC_CRT_LINKAGE STREQUAL "dynamic")
    add_definitions(-D_AFXDLL)
    # for shared MFC library
    set(CMAKE_MFC_UNICODE_FLAG 2)
  else()
    # for static MFC library
    set(CMAKE_MFC_UNICODE_FLAG 1)
  endif()

  # CMake's FindMFC doesn't cooperate with unicode version, use our version instead
  # https://cmake.org/pipermail/cmake-developers/2014-November/023656.html
  find_package(MFCUnicode REQUIRED)
  if (NOT MFC_UNICODE_FOUND)
    message(FATAL_ERROR "Required component MFC/AFX is not installed.")
  endif()
  set(GUI_USE_FILE "")
  set(GUI_INCLUDE_DIRS "")
  set(GUI_DEFINITIONS "")
  set(GUI_C_CXX_FLAGS "")
  set(GUI_LIBRARY_DIRS "")
  set(GUI_LIBRARIES "")
elseif (APPLE AND GUI)
  find_library(COCOA_LIBRARY Cocoa REQUIRED)
  # Locate Interface Builder Tool, needed to build things like Storyboards outside of Xcode.
  if(NOT ${CMAKE_GENERATOR} MATCHES "^Xcode.*")
    # Make sure we can find the 'ibtool' program. If we can NOT find it we skip generation of this project.
    find_program(IBTOOL ibtool HINTS "/usr/bin")
    if(${IBTOOL} STREQUAL "IBTOOL-NOTFOUND")
      message(SEND_ERROR "ibtool can not be found and is needed to compile the .xib files. "
                         "It should have been installed with the Apple developer tools. ")
    endif()
  endif()

  set(GUI_USE_FILE "")
  set(GUI_INCLUDE_DIRS "")
  set(GUI_DEFINITIONS "")
  set(GUI_C_CXX_FLAGS "")
  set(GUI_LIBRARY_DIRS "")
  set(GUI_LIBRARIES "${COCOA_LIBRARY}")
elseif (GUI)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK REQUIRED gtk+-3.0)
  pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
  set(GUI_USE_FILE "")
  set(GUI_INCLUDE_DIRS ${GTK_INCLUDE_DIRS})
  set(GUI_DEFINITIONS "")
  set(GUI_C_CXX_FLAGS ${GTK_CFLAGS} ${GTK_CFLAGS_OTHER})
  set(GUI_LIBRARY_DIRS ${GTK_LIBRARY_DIRS})
  set(GUI_LIBRARIES ${GTK_LIBRARIES} ${FONTCONFIG_LIBRARIES})
endif()

# fix lto warnings on mac
if ((COMPILER_GCC OR COMPILER_CLANG) AND NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -fvisibility=hidden")
endif()

# abseil-cpp
set(ABSL_PROPAGATE_CXX_STD "ON" CACHE STRING "")
add_subdirectory(third_party/abseil-cpp)
include_directories(third_party/abseil-cpp SYSTEM)

set(SUPPORT_LIBS
  ${SUPPORT_LIBS}
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
  )

# asio
add_definitions(-DASIO_STANDALONE)
add_definitions(-DASIO_NO_TYPEID)
add_definitions(-DASIO_NO_EXCEPTIONS)
include_directories(third_party/asio/asio/include SYSTEM)

# boringssl
add_subdirectory(third_party/boringssl/src)
add_definitions(-DHAVE_BORINGSSL)
include_directories(BEFORE third_party/boringssl/src/include SYSTEM)
set(SUPPORT_LIBS ${SUPPORT_LIBS} crypto ssl)
foreach(source ${BCM_ASM_SOURCES})
  set_source_files_properties(${source} PROPERTIES GENERATED true)
endforeach()

set_target_properties(bssl PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(decrepit PROPERTIES EXCLUDE_FROM_ALL TRUE)

# rapidjson
include_directories(third_party/rapidjson/include SYSTEM)

# googletest
include_directories(third_party/googletest/googletest/include SYSTEM)

# xxhash
include_directories(third_party/xxhash SYSTEM)
add_library(xxhash STATIC third_party/xxhash/xxhash.c)

if (COMPILER_MSVC)
  set_target_properties(xxhash PROPERTIES
    COMPILE_FLAGS "/wd4255")
endif()

set(SUPPORT_LIBS xxhash ${SUPPORT_LIBS})

# protobuf
set(protobuf_INSTALL "OFF" CACHE STRING "")
set(protobuf_DISABLE_RTTI "ON" CACHE STRING "")
if (CMAKE_MSVC_CRT_LINKAGE STREQUAL "dynamic")
  set(protobuf_MSVC_STATIC_RUNTIME "OFF" CACHE STRING "")
else()
  set(protobuf_MSVC_STATIC_RUNTIME "ON" CACHE STRING "")
endif()
set(protobuf_BUILD_TESTS "OFF" CACHE STRING "")
set(protobuf_WITH_ZLIB "OFF" CACHE STRING "")
add_subdirectory(third_party/protobuf/cmake)
include_directories(third_party/protobuf/src SYSTEM)
# required because it is not bound with libproto/libproto-lite targets
add_definitions(-DGOOGLE_PROTOBUF_NO_RTTI=1)

# fix protoc's path
set_target_properties(protoc PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

# exclude non-native protoc from being build
set_target_properties(protoc PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(libprotoc PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(libprotobuf PROPERTIES EXCLUDE_FROM_ALL TRUE)

# protobuf helper function
include(NativeTool)
add_native_protoc(protoc yass)
include(Protobuf)
# *****************************************************************************************
#           Compiler warning flags
# *****************************************************************************************
if (COMPILER_GCC OR COMPILER_CLANG)
  # Note clang-cl is odd and sets both CLANG and MSVC. We base our configuration
  # primarily on our normal Clang one.
  set(C_CXX_FLAGS "-Werror -Wformat=2 -Wsign-compare -Wmissing-field-initializers -Wwrite-strings -Wvla -Wno-shadow")

  if (MSVC)
    # clang-cl sets different default warnings than clang. It also treats -Wall
    # as -Weverything, to match MSVC. Instead -W3 is the alias for -Wall.
    # See http://llvm.org/viewvc/llvm-project?view=revision&revision=319116
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -W3 -Wno-unused-parameter")
    # googletest suppresses warning C4996 via a pragma, but clang-cl does not
    # honor it. Suppress it here to compensate. See https://crbug.com/772117.
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wno-deprecated-declarations")
  elseif (ENABLE_CLANG_TIDY)
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -fno-common")
  else()
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wall -fno-common")
  endif()

  if(COMPILER_CLANG)
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wnewline-eof -fcolor-diagnostics")
  else()
    # GCC (at least 4.8.4) has a bug where it'll find unreachable free() calls
    # and declare that the code is trying to free a stack pointer.
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wno-free-nonheap-object")
  endif()

  if (COMPILER_CLANG OR NOT "7.0.0" VERSION_GREATER CMAKE_C_COMPILER_VERSION)
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wimplicit-fallthrough")
  endif()

  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-warning-option")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS} -Wmissing-prototypes -Wold-style-definition -Wstrict-prototypes")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_CXX_FLAGS} -Wmissing-declarations")

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

  if (MSVC)
    ## Set Source and Excutable character sets to UTF-8
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
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
  if (COMPILER_CLANG)
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
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-maybe-uninitialized -Wno-deprecated-declarations")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized -Wno-deprecated-declarations")
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
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wthread-safety")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wthread-safety")
  endif()

  ## When compiling Objective-C, warns if a method is used whose
  ## availability is newer than the deployment target.
  if (APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunguarded-availability")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunguarded-availability")
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
      )
  set(MSVC_LEVEL4_WARNINGS_LIST
      # See https://connect.microsoft.com/VisualStudio/feedback/details/1217660/warning-c4265-when-using-functional-header
      "C4265" # class has virtual functions, but destructor is not virtual
      )
  string(REPLACE "C" " -wd" MSVC_DISABLED_WARNINGS_STR
                            ${MSVC_DISABLED_WARNINGS_LIST})
  string(REPLACE "C" " -w4" MSVC_LEVEL4_WARNINGS_STR
                            ${MSVC_LEVEL4_WARNINGS_LIST})

  set(CMAKE_C_FLAGS   "-utf-8 ${MSVC_DISABLED_WARNINGS_STR} ${MSVC_LEVEL4_WARNINGS_STR}")
  set(CMAKE_CXX_FLAGS "-utf-8 ${MSVC_DISABLED_WARNINGS_STR} ${MSVC_LEVEL4_WARNINGS_STR}")

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
#           Compiler compiler and link flags
# *****************************************************************************************

# Enable position-independent code globally. This is needed because
# some library targets are OBJECT libraries.
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

if (NOT MSVC)
    # Since we build with -std=c* and not -std=gnu*, _GNU_SOURCE will not be
    # defined by the compiler.  However, lots of code relies on the
    # non-standard features that _GNU_SOURCE enables, so define it manually.
    if (UNIX AND NOT APPLE)
      add_definitions(-D_GNU_SOURCE)
    endif()

    # Necessary for clone().
    if (ANDROID)
      add_definitions(-D__GNU_SOURCE=1)
    endif()

    #The system libc++ on Mac doesn't have aligned allocation in C++17.
    if (APPLE)
      add_definitions(-D_LIBCPP_HAS_NO_ALIGNED_ALLOCATION)
    endif()

    ## optimization flags
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")

    ## can gc eliminate any unused functions and data items
    if (NOT APPLE)
        set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections")
    endif()

    ## standard C++ library
    if(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()

    if (APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-dead_strip")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip")
    else()
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
    endif()

    if (APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-no_data_in_code_info -Wl,-no_function_starts")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,-no_data_in_code_info -Wl,-no_function_starts")
    endif()

    ## adopt LTO
    if (APPLE)
        message(STATUS "Compiling with LTO")
        set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -flto -fwhole-program-vtables")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -fwhole-program-vtables")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -flto -fwhole-program-vtables")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto -fwhole-program-vtables")
    endif()

    ## definitions
    add_definitions(-D_FILE_OFFSET_BITS=64)
    add_definitions(-D_LARGEFILE_SOURCE)
    add_definitions(-D_LARGEFILE64_SOURCE)
    add_definitions(-D__STDC_CONSTANT_MACROS)
    add_definitions(-D__STDC_FORMAT_MACROS)

    ## pthread
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -pthread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

    ## faster sub-process communiation
    if (NOT COMPILER_CLANG)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe")
    endif()

    if (NOT APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,noexecstack -Wl,-z,relro")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,noexecstack -Wl,-z,relro")
    endif()

    ## disable rtti
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

    ## disable exceptions
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

    ## not omit stack frame pointer for release
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")

    ## stack protector
    if (APPLE)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -fstack-protector-strong")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -fstack-protector")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -fstack-protector")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector")
    endif()

    ## update build-id
    if (NOT APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--build-id=sha1")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--build-id=sha1")
    endif()

    ## unwind-tables
    if (NOT APPLE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -funwind-tables")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funwind-tables")
    endif()

    ## fortify source code. Also, fortified build may fail when optimizations are
    ## disabled, so only do that for Release build.
    if (${CMAKE_BUILD_TYPE} MATCHES Release)
        add_definitions(-D_FORTIFY_SOURCE=2)
    endif()

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
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO
        )
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/GR" "/GR-" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()

    ## Disable RTTI
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

    ## Disable exceptions in the STL
    add_definitions(-D_HAS_EXCEPTIONS=0)

    ## C4530, The code uses C++ exception handling,
    ## but /EHsc wasn't included in the compiler options
    ## https://docs.microsoft.com/en-us/cpp/build/reference/eh-exception-handling-model?view=msvc-170
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

    ## Optimization flags
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Zc:inline /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zc:inline /DNDEBUG")

    ## Generate debug info, Debug information in the .obj files for clang.
    ## Produce PDB file for msvc, no edit and continue.
    if (COMPILER_CLANG)
      set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Z7")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Z7")
    else()
      set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Zi")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zi")
    endif()

    # Tell linker to include symbol data
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")

    ## Remove unreferenced data
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF")

    if (NOT COMPILER_CLANG)
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO /OPT:ICF")
      set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO /OPT:ICF")
    endif()

    # Tell linker to include FIXUP information symbol data
    if (NOT COMPILER_CLANG)
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /PROFILE")
      set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /PROFILE")
    endif()

    ## Disable LTCG for static libraries because this setting introduces ABI incompatibility between minor compiler versions
    if (NOT CMAKE_MSVC_CRT_LINKAGE STREQUAL "static" AND NOT COMPILER_CLANG)
      set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /GL")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
      set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
      set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    endif()

    if (COMPILER_CLANG AND LINKER_LLD)
      message(STATUS "Compiling with LTO")
      set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -flto")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -flto")
      set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
    endif()

    # lld's uac manifest is outdated and incompatible with windows xp
    if (LINKER_LLD)
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFESTUAC:NO")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFESTUAC:NO")
    endif()

    ## not omit stack frame pointer
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Oy-")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oy-")

    ## supress some warnings
    if (COMPILER_CLANG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-microsoft-exception-spec")
    endif(COMPILER_CLANG)

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
    foreach(DELAY_LDFLAG ${DELAY_LDFLAGS})
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${DELAY_LDFLAG}")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${DELAY_LDFLAG}")
    endforeach()
endif()

# *****************************************************************************************
#           Debug facilities
# *****************************************************************************************

if (UNIX AND ${COMPILER_CLANG} AND
    CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fcolor-diagnostics")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
endif()

if (UNIX AND ${COMPILER_GCC} AND
    CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()

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
endif()

if(CFI)
  if(NOT COMPILER_CLANG)
    message(FATAL_ERROR "Cannot enable CFI unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=cfi -fno-sanitize-trap=cfi -flto=thin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=cfi -fno-sanitize-trap=cfi -flto=thin")
  # We use Chromium's copy of clang, which requires -fuse-ld=lld if building
  # with -flto. That, in turn, can't handle -ggdb.
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
  string(REPLACE "-ggdb" "-g" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REPLACE "-ggdb" "-g" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
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
endif()

if(GCOV)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if (NOT MSAN AND NOT ASAN AND NOT TSAN AND NOT UBSAN)
  if (NOT APPLE AND NOT MSVC)
    if (COMPILER_CLANG)
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld -Wl,--icf=all")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -Wl,--icf=all")
    elseif ()
      # Gold doesn't respect section alignment and breaks gcc builds with icf
      # https://sourceware.org/bugzilla/show_bug.cgi?id=17704
      # Landed upstream Fri, 21 Oct 2016 - is in 2.28, such as CentOS 7
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold -Wl,--icf=all")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold -Wl,--icf=all")
    endif()
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,defs -Wl,--as-needed")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,defs -Wl,--as-needed")
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
#           Zlib Library
# *****************************************************************************************
set (zlib_SRCS
  third_party/zlib/adler32.c
  third_party/zlib/chromeconf.h
  third_party/zlib/compress.c
  third_party/zlib/contrib/optimizations/insert_string.h
  third_party/zlib/cpu_features.c
  third_party/zlib/cpu_features.h
  third_party/zlib/crc32.c
  third_party/zlib/crc32.h
  third_party/zlib/deflate.c
  third_party/zlib/deflate.h
  third_party/zlib/gzclose.c
  third_party/zlib/gzguts.h
  third_party/zlib/gzlib.c
  third_party/zlib/gzread.c
  third_party/zlib/gzwrite.c
  third_party/zlib/infback.c
  third_party/zlib/inffast.c
  third_party/zlib/inffast.h
  third_party/zlib/inffixed.h
  third_party/zlib/inflate.h
  third_party/zlib/inftrees.c
  third_party/zlib/inftrees.h
  third_party/zlib/trees.c
  third_party/zlib/trees.h
  third_party/zlib/uncompr.c
  third_party/zlib/zconf.h
  third_party/zlib/zlib.h
  third_party/zlib/zutil.c
  third_party/zlib/zutil.h
  # TODO check simd
  third_party/zlib/inflate.c
  )

add_library(zlib STATIC
  ${zlib_SRCS}
  )
target_include_directories(zlib PUBLIC
  third_party/zlib
  )
target_compile_definitions(zlib PRIVATE "-DCPU_NO_SIMD")
if (COMPILER_CLANG)
  set_target_properties(zlib PROPERTIES
    COMPILE_FLAGS "-Wno-missing-prototypes -Wno-implicit-fallthrough -Wno-format-nonliteral -Wno-sign-compare -Wno-strict-prototypes")
elseif (COMPILER_GCC)
  set_target_properties(zlib PROPERTIES
    COMPILE_FLAGS "-Wno-missing-prototypes -Wno-implicit-fallthrough -Wno-format-nonliteral -Wno-sign-compare -Wno-strict-prototypes -Wno-old-style-definition -Wno-unused-variable")
elseif (COMPILER_MSVC)
  set_target_properties(zlib PROPERTIES
    COMPILE_FLAGS "/wd4131 /wd4255 /wd4245 /wd4244 /wd4100 /wd4702 /wd4127")
endif()

set(SUPPORT_LIBS zlib ${SUPPORT_LIBS})

# *****************************************************************************************
#           QUICHE Library
# *****************************************************************************************

set(quiche_SRCS
  third_party/icu/icu_utf.h
  third_party/mozilla/url_parse.cc
  third_party/mozilla/url_parse.h
  third_party/chromium/net/base/escape.cc
  third_party/chromium/net/base/escape.h
  third_party/chromium/net/base/ip_address.cc
  third_party/chromium/net/base/ip_address.h
  third_party/chromium/net/base/parse_number.cc
  third_party/chromium/net/base/parse_number.h
  third_party/chromium/net/base/url_util.cc
  third_party/chromium/net/base/url_util.h
  third_party/chromium/net/base/net_errors.cc
  third_party/chromium/net/base/net_errors.h
  third_party/chromium/net/base/net_error_list.h
  third_party/chromium/net/quic/platform/impl/quic_hostname_utils_impl.cc
  third_party/chromium/net/quic/platform/impl/quic_hostname_utils_impl.h
  third_party/chromium/net/quic/platform/impl/quic_flags_impl.cc
  third_party/chromium/net/quic/platform/impl/quic_mem_slice_impl.cc
  third_party/chromium/net/quic/platform/impl/quic_mutex_impl.cc
  third_party/chromium/net/quic/platform/impl/quic_mutex_impl.h
  third_party/chromium/net/quiche/common/platform/impl/quiche_flags_impl.cc
  third_party/chromium/net/quiche/common/platform/impl/quiche_time_utils_impl.cc
  third_party/chromium/url/gurl.cc
  third_party/chromium/url/gurl.h
  third_party/chromium/url/scheme_host_port.cc
  third_party/chromium/url/scheme_host_port.h
  third_party/chromium/url/url_canon_etc.cc
  third_party/chromium/url/url_canon_filesystemurl.cc
  third_party/chromium/url/url_canon_fileurl.cc
  third_party/chromium/url/url_canon_mailtourl.cc
  third_party/chromium/url/url_canon_pathurl.cc
  third_party/chromium/url/url_canon_relative.cc
  third_party/chromium/url/url_canon_stdurl.cc
  third_party/chromium/url/url_canon_host.cc
  third_party/chromium/url/url_canon_path.cc
  third_party/chromium/url/url_canon_query.cc
  third_party/chromium/url/url_canon.cc
  third_party/chromium/url/url_canon.h
  third_party/chromium/url/url_canon_internal.cc
  third_party/chromium/url/url_canon_internal.h
  third_party/chromium/url/url_canon_ip.cc
  third_party/chromium/url/url_canon_ip.h
  third_party/chromium/url/url_canon_stdstring.cc
  third_party/chromium/url/url_canon_stdstring.h
  third_party/chromium/url/url_constants.cc
  third_party/chromium/url/url_constants.h
  third_party/chromium/url/url_util.cc
  third_party/chromium/url/url_util.h
  third_party/chromium/url/url_util_internal.h
  third_party/chromium/url/url_parse_file.cc
  third_party/chromium/url/url_idna_icu.cc
  third_party/chromium/url/origin.cc
  third_party/chromium/url/origin.h
  third_party/quiche/src/common/platform/api/quiche_bug_tracker.h
  third_party/quiche/src/common/platform/api/quiche_export.h
  third_party/quiche/src/common/platform/api/quiche_flag_utils.h
  third_party/quiche/src/common/platform/api/quiche_flags.h
  third_party/quiche/src/common/platform/api/quiche_logging.h
  third_party/quiche/src/common/platform/api/quiche_prefetch.h
  third_party/quiche/src/common/platform/api/quiche_thread_local.h
  third_party/quiche/src/common/platform/api/quiche_time_utils.h
  third_party/quiche/src/common/platform/api/quiche_url_utils.h
  third_party/quiche/src/common/platform/default/quiche_platform_impl/quiche_prefetch_impl.h
  third_party/quiche/src/common/print_elements.h
  third_party/quiche/src/common/quiche_circular_deque.h
  third_party/quiche/src/common/quiche_data_reader.cc
  third_party/quiche/src/common/quiche_data_reader.h
  third_party/quiche/src/common/quiche_data_writer.cc
  third_party/quiche/src/common/quiche_data_writer.h
  third_party/quiche/src/common/quiche_endian.h
  third_party/quiche/src/common/quiche_linked_hash_map.h
  third_party/quiche/src/common/quiche_text_utils.cc
  third_party/quiche/src/common/quiche_text_utils.h
  third_party/quiche/src/http2/adapter/data_source.h
  third_party/quiche/src/http2/adapter/event_forwarder.cc
  third_party/quiche/src/http2/adapter/event_forwarder.h
  third_party/quiche/src/http2/adapter/header_validator.cc
  third_party/quiche/src/http2/adapter/header_validator.h
  third_party/quiche/src/http2/adapter/http2_adapter.h
  third_party/quiche/src/http2/adapter/http2_protocol.cc
  third_party/quiche/src/http2/adapter/http2_protocol.h
  third_party/quiche/src/http2/adapter/http2_session.h
  third_party/quiche/src/http2/adapter/http2_util.cc
  third_party/quiche/src/http2/adapter/http2_util.h
  third_party/quiche/src/http2/adapter/http2_visitor_interface.h
  third_party/quiche/src/http2/adapter/oghttp2_adapter.cc
  third_party/quiche/src/http2/adapter/oghttp2_adapter.h
  third_party/quiche/src/http2/adapter/oghttp2_session.cc
  third_party/quiche/src/http2/adapter/oghttp2_session.h
  third_party/quiche/src/http2/adapter/oghttp2_util.cc
  third_party/quiche/src/http2/adapter/oghttp2_util.h
  third_party/quiche/src/http2/adapter/window_manager.cc
  third_party/quiche/src/http2/adapter/window_manager.h
  third_party/quiche/src/http2/core/http2_trace_logging.cc
  third_party/quiche/src/http2/core/http2_trace_logging.h
  third_party/quiche/src/http2/core/priority_write_scheduler.h
  third_party/quiche/src/http2/core/write_scheduler.h
  third_party/quiche/src/http2/decoder/decode_buffer.cc
  third_party/quiche/src/http2/decoder/decode_buffer.h
  third_party/quiche/src/http2/decoder/decode_http2_structures.cc
  third_party/quiche/src/http2/decoder/decode_http2_structures.h
  third_party/quiche/src/http2/decoder/decode_status.cc
  third_party/quiche/src/http2/decoder/decode_status.h
  third_party/quiche/src/http2/decoder/frame_decoder_state.cc
  third_party/quiche/src/http2/decoder/frame_decoder_state.h
  third_party/quiche/src/http2/decoder/http2_frame_decoder.cc
  third_party/quiche/src/http2/decoder/http2_frame_decoder.h
  third_party/quiche/src/http2/decoder/http2_frame_decoder_listener.cc
  third_party/quiche/src/http2/decoder/http2_frame_decoder_listener.h
  third_party/quiche/src/http2/decoder/http2_structure_decoder.cc
  third_party/quiche/src/http2/decoder/http2_structure_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/altsvc_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/altsvc_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/continuation_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/continuation_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/data_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/data_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/goaway_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/goaway_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/headers_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/headers_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/ping_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/ping_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/priority_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/priority_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/priority_update_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/priority_update_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/push_promise_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/push_promise_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/rst_stream_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/rst_stream_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/settings_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/settings_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/unknown_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/unknown_payload_decoder.h
  third_party/quiche/src/http2/decoder/payload_decoders/window_update_payload_decoder.cc
  third_party/quiche/src/http2/decoder/payload_decoders/window_update_payload_decoder.h
  third_party/quiche/src/http2/hpack/decoder/hpack_block_decoder.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_block_decoder.h
  third_party/quiche/src/http2/hpack/decoder/hpack_decoder.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_decoder.h
  third_party/quiche/src/http2/hpack/decoder/hpack_decoder_listener.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_decoder_listener.h
  third_party/quiche/src/http2/hpack/decoder/hpack_decoder_state.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_decoder_state.h
  third_party/quiche/src/http2/hpack/decoder/hpack_decoder_string_buffer.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_decoder_string_buffer.h
  third_party/quiche/src/http2/hpack/decoder/hpack_decoder_tables.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_decoder_tables.h
  third_party/quiche/src/http2/hpack/decoder/hpack_decoding_error.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_decoding_error.h
  third_party/quiche/src/http2/hpack/decoder/hpack_entry_decoder.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_entry_decoder.h
  third_party/quiche/src/http2/hpack/decoder/hpack_entry_decoder_listener.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_entry_decoder_listener.h
  third_party/quiche/src/http2/hpack/decoder/hpack_entry_type_decoder.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_entry_type_decoder.h
  third_party/quiche/src/http2/hpack/decoder/hpack_string_decoder.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_string_decoder.h
  third_party/quiche/src/http2/hpack/decoder/hpack_string_decoder_listener.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_string_decoder_listener.h
  third_party/quiche/src/http2/hpack/decoder/hpack_whole_entry_buffer.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_whole_entry_buffer.h
  third_party/quiche/src/http2/hpack/decoder/hpack_whole_entry_listener.cc
  third_party/quiche/src/http2/hpack/decoder/hpack_whole_entry_listener.h
  third_party/quiche/src/http2/hpack/hpack_static_table_entries.inc
  third_party/quiche/src/http2/hpack/http2_hpack_constants.cc
  third_party/quiche/src/http2/hpack/http2_hpack_constants.h
  third_party/quiche/src/http2/hpack/huffman/hpack_huffman_decoder.cc
  third_party/quiche/src/http2/hpack/huffman/hpack_huffman_decoder.h
  third_party/quiche/src/http2/hpack/huffman/hpack_huffman_encoder.cc
  third_party/quiche/src/http2/hpack/huffman/hpack_huffman_encoder.h
  third_party/quiche/src/http2/hpack/huffman/huffman_spec_tables.cc
  third_party/quiche/src/http2/hpack/huffman/huffman_spec_tables.h
  third_party/quiche/src/http2/hpack/varint/hpack_varint_decoder.cc
  third_party/quiche/src/http2/hpack/varint/hpack_varint_decoder.h
  third_party/quiche/src/http2/hpack/varint/hpack_varint_encoder.cc
  third_party/quiche/src/http2/hpack/varint/hpack_varint_encoder.h
  third_party/quiche/src/http2/http2_constants.cc
  third_party/quiche/src/http2/http2_constants.h
  third_party/quiche/src/http2/http2_structures.cc
  third_party/quiche/src/http2/http2_structures.h
  third_party/quiche/src/http2/platform/api/http2_bug_tracker.h
  third_party/quiche/src/http2/platform/api/http2_flag_utils.h
  third_party/quiche/src/http2/platform/api/http2_flags.h
  third_party/quiche/src/http2/platform/api/http2_logging.h
  third_party/quiche/src/quic/core/congestion_control/bandwidth_sampler.cc
  third_party/quiche/src/quic/core/congestion_control/bandwidth_sampler.h
  third_party/quiche/src/quic/core/congestion_control/bbr2_drain.cc
  third_party/quiche/src/quic/core/congestion_control/bbr2_drain.h
  third_party/quiche/src/quic/core/congestion_control/bbr2_misc.cc
  third_party/quiche/src/quic/core/congestion_control/bbr2_misc.h
  third_party/quiche/src/quic/core/congestion_control/bbr2_probe_bw.cc
  third_party/quiche/src/quic/core/congestion_control/bbr2_probe_bw.h
  third_party/quiche/src/quic/core/congestion_control/bbr2_probe_rtt.cc
  third_party/quiche/src/quic/core/congestion_control/bbr2_probe_rtt.h
  third_party/quiche/src/quic/core/congestion_control/bbr2_sender.cc
  third_party/quiche/src/quic/core/congestion_control/bbr2_sender.h
  third_party/quiche/src/quic/core/congestion_control/bbr2_startup.cc
  third_party/quiche/src/quic/core/congestion_control/bbr2_startup.h
  third_party/quiche/src/quic/core/congestion_control/bbr_sender.cc
  third_party/quiche/src/quic/core/congestion_control/bbr_sender.h
  third_party/quiche/src/quic/core/congestion_control/cubic_bytes.cc
  third_party/quiche/src/quic/core/congestion_control/cubic_bytes.h
  third_party/quiche/src/quic/core/congestion_control/general_loss_algorithm.cc
  third_party/quiche/src/quic/core/congestion_control/general_loss_algorithm.h
  third_party/quiche/src/quic/core/congestion_control/hybrid_slow_start.cc
  third_party/quiche/src/quic/core/congestion_control/hybrid_slow_start.h
  third_party/quiche/src/quic/core/congestion_control/loss_detection_interface.h
  third_party/quiche/src/quic/core/congestion_control/pacing_sender.cc
  third_party/quiche/src/quic/core/congestion_control/pacing_sender.h
  third_party/quiche/src/quic/core/congestion_control/prr_sender.cc
  third_party/quiche/src/quic/core/congestion_control/prr_sender.h
  third_party/quiche/src/quic/core/congestion_control/rtt_stats.cc
  third_party/quiche/src/quic/core/congestion_control/rtt_stats.h
  third_party/quiche/src/quic/core/congestion_control/send_algorithm_interface.cc
  third_party/quiche/src/quic/core/congestion_control/send_algorithm_interface.h
  third_party/quiche/src/quic/core/congestion_control/tcp_cubic_sender_bytes.cc
  third_party/quiche/src/quic/core/congestion_control/tcp_cubic_sender_bytes.h
  third_party/quiche/src/quic/core/congestion_control/uber_loss_algorithm.cc
  third_party/quiche/src/quic/core/congestion_control/uber_loss_algorithm.h
  third_party/quiche/src/quic/core/congestion_control/windowed_filter.h
  third_party/quiche/src/quic/core/crypto/aead_base_decrypter.cc
  third_party/quiche/src/quic/core/crypto/aead_base_decrypter.h
  third_party/quiche/src/quic/core/crypto/aead_base_encrypter.cc
  third_party/quiche/src/quic/core/crypto/aead_base_encrypter.h
  third_party/quiche/src/quic/core/crypto/aes_128_gcm_12_decrypter.cc
  third_party/quiche/src/quic/core/crypto/aes_128_gcm_12_decrypter.h
  third_party/quiche/src/quic/core/crypto/aes_128_gcm_12_encrypter.cc
  third_party/quiche/src/quic/core/crypto/aes_128_gcm_12_encrypter.h
  third_party/quiche/src/quic/core/crypto/aes_128_gcm_decrypter.cc
  third_party/quiche/src/quic/core/crypto/aes_128_gcm_decrypter.h
  third_party/quiche/src/quic/core/crypto/aes_128_gcm_encrypter.cc
  third_party/quiche/src/quic/core/crypto/aes_128_gcm_encrypter.h
  third_party/quiche/src/quic/core/crypto/aes_256_gcm_decrypter.cc
  third_party/quiche/src/quic/core/crypto/aes_256_gcm_decrypter.h
  third_party/quiche/src/quic/core/crypto/aes_256_gcm_encrypter.cc
  third_party/quiche/src/quic/core/crypto/aes_256_gcm_encrypter.h
  third_party/quiche/src/quic/core/crypto/aes_base_decrypter.cc
  third_party/quiche/src/quic/core/crypto/aes_base_decrypter.h
  third_party/quiche/src/quic/core/crypto/aes_base_encrypter.cc
  third_party/quiche/src/quic/core/crypto/aes_base_encrypter.h
  third_party/quiche/src/quic/core/crypto/boring_utils.h
  third_party/quiche/src/quic/core/crypto/cert_compressor.cc
  third_party/quiche/src/quic/core/crypto/cert_compressor.h
  third_party/quiche/src/quic/core/crypto/certificate_view.cc
  third_party/quiche/src/quic/core/crypto/certificate_view.h
  third_party/quiche/src/quic/core/crypto/chacha20_poly1305_decrypter.cc
  third_party/quiche/src/quic/core/crypto/chacha20_poly1305_decrypter.h
  third_party/quiche/src/quic/core/crypto/chacha20_poly1305_encrypter.cc
  third_party/quiche/src/quic/core/crypto/chacha20_poly1305_encrypter.h
  third_party/quiche/src/quic/core/crypto/chacha20_poly1305_tls_decrypter.cc
  third_party/quiche/src/quic/core/crypto/chacha20_poly1305_tls_decrypter.h
  third_party/quiche/src/quic/core/crypto/chacha20_poly1305_tls_encrypter.cc
  third_party/quiche/src/quic/core/crypto/chacha20_poly1305_tls_encrypter.h
  third_party/quiche/src/quic/core/crypto/chacha_base_decrypter.cc
  third_party/quiche/src/quic/core/crypto/chacha_base_decrypter.h
  third_party/quiche/src/quic/core/crypto/chacha_base_encrypter.cc
  third_party/quiche/src/quic/core/crypto/chacha_base_encrypter.h
  third_party/quiche/src/quic/core/crypto/channel_id.cc
  third_party/quiche/src/quic/core/crypto/channel_id.h
  third_party/quiche/src/quic/core/crypto/client_proof_source.cc
  third_party/quiche/src/quic/core/crypto/client_proof_source.h
  third_party/quiche/src/quic/core/crypto/common_cert_set.cc
  third_party/quiche/src/quic/core/crypto/common_cert_set.h
  third_party/quiche/src/quic/core/crypto/crypto_framer.cc
  third_party/quiche/src/quic/core/crypto/crypto_framer.h
  third_party/quiche/src/quic/core/crypto/crypto_handshake.cc
  third_party/quiche/src/quic/core/crypto/crypto_handshake.h
  third_party/quiche/src/quic/core/crypto/crypto_handshake_message.cc
  third_party/quiche/src/quic/core/crypto/crypto_handshake_message.h
  third_party/quiche/src/quic/core/crypto/crypto_message_parser.h
  third_party/quiche/src/quic/core/crypto/crypto_protocol.h
  third_party/quiche/src/quic/core/crypto/crypto_secret_boxer.cc
  third_party/quiche/src/quic/core/crypto/crypto_secret_boxer.h
  third_party/quiche/src/quic/core/crypto/crypto_utils.cc
  third_party/quiche/src/quic/core/crypto/crypto_utils.h
  third_party/quiche/src/quic/core/crypto/curve25519_key_exchange.cc
  third_party/quiche/src/quic/core/crypto/curve25519_key_exchange.h
  third_party/quiche/src/quic/core/crypto/key_exchange.cc
  third_party/quiche/src/quic/core/crypto/key_exchange.h
  third_party/quiche/src/quic/core/crypto/null_decrypter.cc
  third_party/quiche/src/quic/core/crypto/null_decrypter.h
  third_party/quiche/src/quic/core/crypto/null_encrypter.cc
  third_party/quiche/src/quic/core/crypto/null_encrypter.h
  third_party/quiche/src/quic/core/crypto/p256_key_exchange.cc
  third_party/quiche/src/quic/core/crypto/p256_key_exchange.h
  third_party/quiche/src/quic/core/crypto/proof_source.cc
  third_party/quiche/src/quic/core/crypto/proof_source.h
  third_party/quiche/src/quic/core/crypto/proof_source_x509.cc
  third_party/quiche/src/quic/core/crypto/proof_source_x509.h
  third_party/quiche/src/quic/core/crypto/proof_verifier.h
  third_party/quiche/src/quic/core/crypto/quic_client_session_cache.cc
  third_party/quiche/src/quic/core/crypto/quic_client_session_cache.h
  third_party/quiche/src/quic/core/crypto/quic_compressed_certs_cache.cc
  third_party/quiche/src/quic/core/crypto/quic_compressed_certs_cache.h
  third_party/quiche/src/quic/core/crypto/quic_crypter.cc
  third_party/quiche/src/quic/core/crypto/quic_crypter.h
  third_party/quiche/src/quic/core/crypto/quic_crypto_client_config.cc
  third_party/quiche/src/quic/core/crypto/quic_crypto_client_config.h
  third_party/quiche/src/quic/core/crypto/quic_crypto_proof.cc
  third_party/quiche/src/quic/core/crypto/quic_crypto_proof.h
  third_party/quiche/src/quic/core/crypto/quic_crypto_server_config.cc
  third_party/quiche/src/quic/core/crypto/quic_crypto_server_config.h
  third_party/quiche/src/quic/core/crypto/quic_decrypter.cc
  third_party/quiche/src/quic/core/crypto/quic_decrypter.h
  third_party/quiche/src/quic/core/crypto/quic_encrypter.cc
  third_party/quiche/src/quic/core/crypto/quic_encrypter.h
  third_party/quiche/src/quic/core/crypto/quic_hkdf.cc
  third_party/quiche/src/quic/core/crypto/quic_hkdf.h
  third_party/quiche/src/quic/core/crypto/quic_random.cc
  third_party/quiche/src/quic/core/crypto/quic_random.h
  third_party/quiche/src/quic/core/crypto/tls_client_connection.cc
  third_party/quiche/src/quic/core/crypto/tls_client_connection.h
  third_party/quiche/src/quic/core/crypto/tls_connection.cc
  third_party/quiche/src/quic/core/crypto/tls_connection.h
  third_party/quiche/src/quic/core/crypto/tls_server_connection.cc
  third_party/quiche/src/quic/core/crypto/tls_server_connection.h
  third_party/quiche/src/quic/core/crypto/transport_parameters.cc
  third_party/quiche/src/quic/core/crypto/transport_parameters.h
  third_party/quiche/src/quic/core/frames/quic_ack_frame.cc
  third_party/quiche/src/quic/core/frames/quic_ack_frame.h
  third_party/quiche/src/quic/core/frames/quic_ack_frequency_frame.cc
  third_party/quiche/src/quic/core/frames/quic_ack_frequency_frame.h
  third_party/quiche/src/quic/core/frames/quic_blocked_frame.cc
  third_party/quiche/src/quic/core/frames/quic_blocked_frame.h
  third_party/quiche/src/quic/core/frames/quic_connection_close_frame.cc
  third_party/quiche/src/quic/core/frames/quic_connection_close_frame.h
  third_party/quiche/src/quic/core/frames/quic_crypto_frame.cc
  third_party/quiche/src/quic/core/frames/quic_crypto_frame.h
  third_party/quiche/src/quic/core/frames/quic_frame.cc
  third_party/quiche/src/quic/core/frames/quic_frame.h
  third_party/quiche/src/quic/core/frames/quic_goaway_frame.cc
  third_party/quiche/src/quic/core/frames/quic_goaway_frame.h
  third_party/quiche/src/quic/core/frames/quic_handshake_done_frame.cc
  third_party/quiche/src/quic/core/frames/quic_handshake_done_frame.h
  third_party/quiche/src/quic/core/frames/quic_inlined_frame.h
  third_party/quiche/src/quic/core/frames/quic_max_streams_frame.cc
  third_party/quiche/src/quic/core/frames/quic_max_streams_frame.h
  third_party/quiche/src/quic/core/frames/quic_message_frame.cc
  third_party/quiche/src/quic/core/frames/quic_message_frame.h
  third_party/quiche/src/quic/core/frames/quic_mtu_discovery_frame.h
  third_party/quiche/src/quic/core/frames/quic_new_connection_id_frame.cc
  third_party/quiche/src/quic/core/frames/quic_new_connection_id_frame.h
  third_party/quiche/src/quic/core/frames/quic_new_token_frame.cc
  third_party/quiche/src/quic/core/frames/quic_new_token_frame.h
  third_party/quiche/src/quic/core/frames/quic_padding_frame.cc
  third_party/quiche/src/quic/core/frames/quic_padding_frame.h
  third_party/quiche/src/quic/core/frames/quic_path_challenge_frame.cc
  third_party/quiche/src/quic/core/frames/quic_path_challenge_frame.h
  third_party/quiche/src/quic/core/frames/quic_path_response_frame.cc
  third_party/quiche/src/quic/core/frames/quic_path_response_frame.h
  third_party/quiche/src/quic/core/frames/quic_ping_frame.cc
  third_party/quiche/src/quic/core/frames/quic_ping_frame.h
  third_party/quiche/src/quic/core/frames/quic_retire_connection_id_frame.cc
  third_party/quiche/src/quic/core/frames/quic_retire_connection_id_frame.h
  third_party/quiche/src/quic/core/frames/quic_rst_stream_frame.cc
  third_party/quiche/src/quic/core/frames/quic_rst_stream_frame.h
  third_party/quiche/src/quic/core/frames/quic_stop_sending_frame.cc
  third_party/quiche/src/quic/core/frames/quic_stop_sending_frame.h
  third_party/quiche/src/quic/core/frames/quic_stop_waiting_frame.cc
  third_party/quiche/src/quic/core/frames/quic_stop_waiting_frame.h
  third_party/quiche/src/quic/core/frames/quic_stream_frame.cc
  third_party/quiche/src/quic/core/frames/quic_stream_frame.h
  third_party/quiche/src/quic/core/frames/quic_streams_blocked_frame.cc
  third_party/quiche/src/quic/core/frames/quic_streams_blocked_frame.h
  third_party/quiche/src/quic/core/frames/quic_window_update_frame.cc
  third_party/quiche/src/quic/core/frames/quic_window_update_frame.h
  third_party/quiche/src/quic/core/handshaker_delegate_interface.h
  third_party/quiche/src/quic/core/http/capsule.cc
  third_party/quiche/src/quic/core/http/capsule.h
  third_party/quiche/src/quic/core/http/http_constants.cc
  third_party/quiche/src/quic/core/http/http_constants.h
  third_party/quiche/src/quic/core/http/http_decoder.cc
  third_party/quiche/src/quic/core/http/http_decoder.h
  third_party/quiche/src/quic/core/http/http_encoder.cc
  third_party/quiche/src/quic/core/http/http_encoder.h
  third_party/quiche/src/quic/core/http/http_frames.h
  third_party/quiche/src/quic/core/http/quic_client_promised_info.cc
  third_party/quiche/src/quic/core/http/quic_client_promised_info.h
  third_party/quiche/src/quic/core/http/quic_client_push_promise_index.cc
  third_party/quiche/src/quic/core/http/quic_client_push_promise_index.h
  third_party/quiche/src/quic/core/http/quic_header_list.cc
  third_party/quiche/src/quic/core/http/quic_header_list.h
  third_party/quiche/src/quic/core/http/quic_headers_stream.cc
  third_party/quiche/src/quic/core/http/quic_headers_stream.h
  third_party/quiche/src/quic/core/http/quic_receive_control_stream.cc
  third_party/quiche/src/quic/core/http/quic_receive_control_stream.h
  third_party/quiche/src/quic/core/http/quic_send_control_stream.cc
  third_party/quiche/src/quic/core/http/quic_send_control_stream.h
  third_party/quiche/src/quic/core/http/quic_server_initiated_spdy_stream.cc
  third_party/quiche/src/quic/core/http/quic_server_initiated_spdy_stream.h
  third_party/quiche/src/quic/core/http/quic_server_session_base.cc
  third_party/quiche/src/quic/core/http/quic_server_session_base.h
  third_party/quiche/src/quic/core/http/quic_spdy_client_session.cc
  third_party/quiche/src/quic/core/http/quic_spdy_client_session.h
  third_party/quiche/src/quic/core/http/quic_spdy_client_session_base.cc
  third_party/quiche/src/quic/core/http/quic_spdy_client_session_base.h
  third_party/quiche/src/quic/core/http/quic_spdy_client_stream.cc
  third_party/quiche/src/quic/core/http/quic_spdy_client_stream.h
  third_party/quiche/src/quic/core/http/quic_spdy_session.cc
  third_party/quiche/src/quic/core/http/quic_spdy_session.h
  third_party/quiche/src/quic/core/http/quic_spdy_stream.cc
  third_party/quiche/src/quic/core/http/quic_spdy_stream.h
  third_party/quiche/src/quic/core/http/quic_spdy_stream_body_manager.cc
  third_party/quiche/src/quic/core/http/quic_spdy_stream_body_manager.h
  third_party/quiche/src/quic/core/http/spdy_server_push_utils.cc
  third_party/quiche/src/quic/core/http/spdy_server_push_utils.h
  third_party/quiche/src/quic/core/http/spdy_utils.cc
  third_party/quiche/src/quic/core/http/spdy_utils.h
  third_party/quiche/src/quic/core/http/web_transport_http3.cc
  third_party/quiche/src/quic/core/http/web_transport_http3.h
  third_party/quiche/src/quic/core/http/web_transport_stream_adapter.cc
  third_party/quiche/src/quic/core/http/web_transport_stream_adapter.h
  third_party/quiche/src/quic/core/legacy_quic_stream_id_manager.cc
  third_party/quiche/src/quic/core/legacy_quic_stream_id_manager.h
  third_party/quiche/src/quic/core/packet_number_indexed_queue.h
  third_party/quiche/src/quic/core/proto/cached_network_parameters_proto.h
  third_party/quiche/src/quic/core/proto/crypto_server_config_proto.h
  third_party/quiche/src/quic/core/proto/source_address_token_proto.h
  third_party/quiche/src/quic/core/qpack/qpack_blocking_manager.cc
  third_party/quiche/src/quic/core/qpack/qpack_blocking_manager.h
  third_party/quiche/src/quic/core/qpack/qpack_decoded_headers_accumulator.cc
  third_party/quiche/src/quic/core/qpack/qpack_decoded_headers_accumulator.h
  third_party/quiche/src/quic/core/qpack/qpack_decoder.cc
  third_party/quiche/src/quic/core/qpack/qpack_decoder.h
  third_party/quiche/src/quic/core/qpack/qpack_decoder_stream_receiver.cc
  third_party/quiche/src/quic/core/qpack/qpack_decoder_stream_receiver.h
  third_party/quiche/src/quic/core/qpack/qpack_decoder_stream_sender.cc
  third_party/quiche/src/quic/core/qpack/qpack_decoder_stream_sender.h
  third_party/quiche/src/quic/core/qpack/qpack_encoder.cc
  third_party/quiche/src/quic/core/qpack/qpack_encoder.h
  third_party/quiche/src/quic/core/qpack/qpack_encoder_stream_receiver.cc
  third_party/quiche/src/quic/core/qpack/qpack_encoder_stream_receiver.h
  third_party/quiche/src/quic/core/qpack/qpack_encoder_stream_sender.cc
  third_party/quiche/src/quic/core/qpack/qpack_encoder_stream_sender.h
  third_party/quiche/src/quic/core/qpack/qpack_header_table.cc
  third_party/quiche/src/quic/core/qpack/qpack_header_table.h
  third_party/quiche/src/quic/core/qpack/qpack_index_conversions.cc
  third_party/quiche/src/quic/core/qpack/qpack_index_conversions.h
  third_party/quiche/src/quic/core/qpack/qpack_instruction_decoder.cc
  third_party/quiche/src/quic/core/qpack/qpack_instruction_decoder.h
  third_party/quiche/src/quic/core/qpack/qpack_instruction_encoder.cc
  third_party/quiche/src/quic/core/qpack/qpack_instruction_encoder.h
  third_party/quiche/src/quic/core/qpack/qpack_instructions.cc
  third_party/quiche/src/quic/core/qpack/qpack_instructions.h
  third_party/quiche/src/quic/core/qpack/qpack_progressive_decoder.cc
  third_party/quiche/src/quic/core/qpack/qpack_progressive_decoder.h
  third_party/quiche/src/quic/core/qpack/qpack_receive_stream.cc
  third_party/quiche/src/quic/core/qpack/qpack_receive_stream.h
  third_party/quiche/src/quic/core/qpack/qpack_required_insert_count.cc
  third_party/quiche/src/quic/core/qpack/qpack_required_insert_count.h
  third_party/quiche/src/quic/core/qpack/qpack_send_stream.cc
  third_party/quiche/src/quic/core/qpack/qpack_send_stream.h
  third_party/quiche/src/quic/core/qpack/qpack_static_table.cc
  third_party/quiche/src/quic/core/qpack/qpack_static_table.h
  third_party/quiche/src/quic/core/qpack/qpack_stream_receiver.h
  third_party/quiche/src/quic/core/qpack/qpack_stream_sender_delegate.h
  third_party/quiche/src/quic/core/qpack/value_splitting_header_list.cc
  third_party/quiche/src/quic/core/qpack/value_splitting_header_list.h
  third_party/quiche/src/quic/core/quic_ack_listener_interface.cc
  third_party/quiche/src/quic/core/quic_ack_listener_interface.h
  third_party/quiche/src/quic/core/quic_alarm.cc
  third_party/quiche/src/quic/core/quic_alarm.h
  third_party/quiche/src/quic/core/quic_alarm_factory.h
  third_party/quiche/src/quic/core/quic_arena_scoped_ptr.h
  third_party/quiche/src/quic/core/quic_bandwidth.cc
  third_party/quiche/src/quic/core/quic_bandwidth.h
  third_party/quiche/src/quic/core/quic_blocked_writer_interface.h
  third_party/quiche/src/quic/core/quic_buffer_allocator.cc
  third_party/quiche/src/quic/core/quic_buffer_allocator.h
  third_party/quiche/src/quic/core/quic_chaos_protector.cc
  third_party/quiche/src/quic/core/quic_chaos_protector.h
  third_party/quiche/src/quic/core/quic_clock.cc
  third_party/quiche/src/quic/core/quic_clock.h
  third_party/quiche/src/quic/core/quic_coalesced_packet.cc
  third_party/quiche/src/quic/core/quic_coalesced_packet.h
  third_party/quiche/src/quic/core/quic_config.cc
  third_party/quiche/src/quic/core/quic_config.h
  third_party/quiche/src/quic/core/quic_connection.cc
  third_party/quiche/src/quic/core/quic_connection.h
  third_party/quiche/src/quic/core/quic_connection_context.cc
  third_party/quiche/src/quic/core/quic_connection_context.h
  third_party/quiche/src/quic/core/quic_connection_id.cc
  third_party/quiche/src/quic/core/quic_connection_id.h
  third_party/quiche/src/quic/core/quic_connection_id_manager.cc
  third_party/quiche/src/quic/core/quic_connection_id_manager.h
  third_party/quiche/src/quic/core/quic_connection_stats.cc
  third_party/quiche/src/quic/core/quic_connection_stats.h
  third_party/quiche/src/quic/core/quic_constants.cc
  third_party/quiche/src/quic/core/quic_constants.h
  third_party/quiche/src/quic/core/quic_control_frame_manager.cc
  third_party/quiche/src/quic/core/quic_control_frame_manager.h
  third_party/quiche/src/quic/core/quic_crypto_client_handshaker.cc
  third_party/quiche/src/quic/core/quic_crypto_client_handshaker.h
  third_party/quiche/src/quic/core/quic_crypto_client_stream.cc
  third_party/quiche/src/quic/core/quic_crypto_client_stream.h
  third_party/quiche/src/quic/core/quic_crypto_handshaker.cc
  third_party/quiche/src/quic/core/quic_crypto_handshaker.h
  third_party/quiche/src/quic/core/quic_crypto_server_stream.cc
  third_party/quiche/src/quic/core/quic_crypto_server_stream.h
  third_party/quiche/src/quic/core/quic_crypto_server_stream_base.cc
  third_party/quiche/src/quic/core/quic_crypto_server_stream_base.h
  third_party/quiche/src/quic/core/quic_crypto_stream.cc
  third_party/quiche/src/quic/core/quic_crypto_stream.h
  third_party/quiche/src/quic/core/quic_data_reader.cc
  third_party/quiche/src/quic/core/quic_data_reader.h
  third_party/quiche/src/quic/core/quic_data_writer.cc
  third_party/quiche/src/quic/core/quic_data_writer.h
  third_party/quiche/src/quic/core/quic_datagram_queue.cc
  third_party/quiche/src/quic/core/quic_datagram_queue.h
  third_party/quiche/src/quic/core/quic_error_codes.cc
  third_party/quiche/src/quic/core/quic_error_codes.h
  third_party/quiche/src/quic/core/quic_flags_list.h
  third_party/quiche/src/quic/core/quic_flow_controller.cc
  third_party/quiche/src/quic/core/quic_flow_controller.h
  third_party/quiche/src/quic/core/quic_framer.cc
  third_party/quiche/src/quic/core/quic_framer.h
  third_party/quiche/src/quic/core/quic_idle_network_detector.cc
  third_party/quiche/src/quic/core/quic_idle_network_detector.h
  third_party/quiche/src/quic/core/quic_interval.h
  third_party/quiche/src/quic/core/quic_interval_deque.h
  third_party/quiche/src/quic/core/quic_interval_set.h
  third_party/quiche/src/quic/core/quic_legacy_version_encapsulator.cc
  third_party/quiche/src/quic/core/quic_legacy_version_encapsulator.h
  third_party/quiche/src/quic/core/quic_lru_cache.h
  third_party/quiche/src/quic/core/quic_mtu_discovery.cc
  third_party/quiche/src/quic/core/quic_mtu_discovery.h
  third_party/quiche/src/quic/core/quic_network_blackhole_detector.cc
  third_party/quiche/src/quic/core/quic_network_blackhole_detector.h
  third_party/quiche/src/quic/core/quic_one_block_arena.h
  third_party/quiche/src/quic/core/quic_packet_creator.cc
  third_party/quiche/src/quic/core/quic_packet_creator.h
  third_party/quiche/src/quic/core/quic_packet_number.cc
  third_party/quiche/src/quic/core/quic_packet_number.h
  third_party/quiche/src/quic/core/quic_packet_writer.h
  third_party/quiche/src/quic/core/quic_packets.cc
  third_party/quiche/src/quic/core/quic_packets.h
  third_party/quiche/src/quic/core/quic_path_validator.cc
  third_party/quiche/src/quic/core/quic_path_validator.h
  third_party/quiche/src/quic/core/quic_protocol_flags_list.h
  third_party/quiche/src/quic/core/quic_received_packet_manager.cc
  third_party/quiche/src/quic/core/quic_received_packet_manager.h
  third_party/quiche/src/quic/core/quic_sent_packet_manager.cc
  third_party/quiche/src/quic/core/quic_sent_packet_manager.h
  third_party/quiche/src/quic/core/quic_server_id.cc
  third_party/quiche/src/quic/core/quic_server_id.h
  third_party/quiche/src/quic/core/quic_session.cc
  third_party/quiche/src/quic/core/quic_session.h
  third_party/quiche/src/quic/core/quic_simple_buffer_allocator.cc
  third_party/quiche/src/quic/core/quic_simple_buffer_allocator.h
  third_party/quiche/src/quic/core/quic_socket_address_coder.cc
  third_party/quiche/src/quic/core/quic_socket_address_coder.h
  third_party/quiche/src/quic/core/quic_stream.cc
  third_party/quiche/src/quic/core/quic_stream.h
  third_party/quiche/src/quic/core/quic_stream_frame_data_producer.h
  third_party/quiche/src/quic/core/quic_stream_id_manager.cc
  third_party/quiche/src/quic/core/quic_stream_id_manager.h
  third_party/quiche/src/quic/core/quic_stream_send_buffer.cc
  third_party/quiche/src/quic/core/quic_stream_send_buffer.h
  third_party/quiche/src/quic/core/quic_stream_sequencer.cc
  third_party/quiche/src/quic/core/quic_stream_sequencer.h
  third_party/quiche/src/quic/core/quic_stream_sequencer_buffer.cc
  third_party/quiche/src/quic/core/quic_stream_sequencer_buffer.h
  third_party/quiche/src/quic/core/quic_sustained_bandwidth_recorder.cc
  third_party/quiche/src/quic/core/quic_sustained_bandwidth_recorder.h
  third_party/quiche/src/quic/core/quic_tag.cc
  third_party/quiche/src/quic/core/quic_tag.h
  third_party/quiche/src/quic/core/quic_time.cc
  third_party/quiche/src/quic/core/quic_time.h
  third_party/quiche/src/quic/core/quic_time_accumulator.h
  third_party/quiche/src/quic/core/quic_transmission_info.cc
  third_party/quiche/src/quic/core/quic_transmission_info.h
  third_party/quiche/src/quic/core/quic_types.cc
  third_party/quiche/src/quic/core/quic_types.h
  third_party/quiche/src/quic/core/quic_unacked_packet_map.cc
  third_party/quiche/src/quic/core/quic_unacked_packet_map.h
  third_party/quiche/src/quic/core/quic_utils.cc
  third_party/quiche/src/quic/core/quic_utils.h
  third_party/quiche/src/quic/core/quic_version_manager.cc
  third_party/quiche/src/quic/core/quic_version_manager.h
  third_party/quiche/src/quic/core/quic_versions.cc
  third_party/quiche/src/quic/core/quic_versions.h
  third_party/quiche/src/quic/core/quic_write_blocked_list.cc
  third_party/quiche/src/quic/core/quic_write_blocked_list.h
  third_party/quiche/src/quic/core/session_notifier_interface.h
  third_party/quiche/src/quic/core/stream_delegate_interface.h
  third_party/quiche/src/quic/core/tls_client_handshaker.cc
  third_party/quiche/src/quic/core/tls_client_handshaker.h
  third_party/quiche/src/quic/core/tls_handshaker.cc
  third_party/quiche/src/quic/core/tls_handshaker.h
  third_party/quiche/src/quic/core/tls_server_handshaker.cc
  third_party/quiche/src/quic/core/tls_server_handshaker.h
  third_party/quiche/src/quic/core/uber_quic_stream_id_manager.cc
  third_party/quiche/src/quic/core/uber_quic_stream_id_manager.h
  third_party/quiche/src/quic/core/uber_received_packet_manager.cc
  third_party/quiche/src/quic/core/uber_received_packet_manager.h
  third_party/quiche/src/quic/platform/api/quic_bug_tracker.h
  third_party/quiche/src/quic/platform/api/quic_client_stats.h
  third_party/quiche/src/quic/platform/api/quic_containers.h
  third_party/quiche/src/quic/platform/api/quic_error_code_wrappers.h
  third_party/quiche/src/quic/platform/api/quic_export.h
  third_party/quiche/src/quic/platform/api/quic_exported_stats.h
  third_party/quiche/src/quic/platform/api/quic_flag_utils.h
  third_party/quiche/src/quic/platform/api/quic_flags.h
  third_party/quiche/src/quic/platform/api/quic_hostname_utils.cc
  third_party/quiche/src/quic/platform/api/quic_hostname_utils.h
  third_party/quiche/src/quic/platform/api/quic_iovec.h
  third_party/quiche/src/quic/platform/api/quic_ip_address.cc
  third_party/quiche/src/quic/platform/api/quic_ip_address.h
  third_party/quiche/src/quic/platform/api/quic_ip_address_family.h
  third_party/quiche/src/quic/platform/api/quic_logging.h
  third_party/quiche/src/quic/platform/api/quic_mem_slice.h
  third_party/quiche/src/quic/platform/api/quic_mem_slice_storage.cc
  third_party/quiche/src/quic/platform/api/quic_mem_slice_storage.h
  third_party/quiche/src/quic/platform/api/quic_mutex.cc
  third_party/quiche/src/quic/platform/api/quic_mutex.h
  third_party/quiche/src/quic/platform/api/quic_reference_counted.h
  third_party/quiche/src/quic/platform/api/quic_server_stats.h
  third_party/quiche/src/quic/platform/api/quic_sleep.h
  third_party/quiche/src/quic/platform/api/quic_socket_address.cc
  third_party/quiche/src/quic/platform/api/quic_socket_address.h
  third_party/quiche/src/quic/platform/api/quic_stack_trace.h
  third_party/quiche/src/quic/platform/api/quic_thread.h
  third_party/quiche/src/quic/quic_transport/quic_transport_client_session.cc
  third_party/quiche/src/quic/quic_transport/quic_transport_client_session.h
  third_party/quiche/src/quic/quic_transport/quic_transport_protocol.h
  third_party/quiche/src/quic/quic_transport/quic_transport_server_session.cc
  third_party/quiche/src/quic/quic_transport/quic_transport_server_session.h
  third_party/quiche/src/quic/quic_transport/quic_transport_session_interface.h
  third_party/quiche/src/quic/quic_transport/quic_transport_stream.cc
  third_party/quiche/src/quic/quic_transport/quic_transport_stream.h
  third_party/quiche/src/quic/quic_transport/web_transport_fingerprint_proof_verifier.cc
  third_party/quiche/src/quic/quic_transport/web_transport_fingerprint_proof_verifier.h
  third_party/quiche/src/spdy/core/hpack/hpack_constants.cc
  third_party/quiche/src/spdy/core/hpack/hpack_constants.h
  third_party/quiche/src/spdy/core/hpack/hpack_decoder_adapter.cc
  third_party/quiche/src/spdy/core/hpack/hpack_decoder_adapter.h
  third_party/quiche/src/spdy/core/hpack/hpack_encoder.cc
  third_party/quiche/src/spdy/core/hpack/hpack_encoder.h
  third_party/quiche/src/spdy/core/hpack/hpack_entry.cc
  third_party/quiche/src/spdy/core/hpack/hpack_entry.h
  third_party/quiche/src/spdy/core/hpack/hpack_header_table.cc
  third_party/quiche/src/spdy/core/hpack/hpack_header_table.h
  third_party/quiche/src/spdy/core/hpack/hpack_output_stream.cc
  third_party/quiche/src/spdy/core/hpack/hpack_output_stream.h
  third_party/quiche/src/spdy/core/hpack/hpack_static_table.cc
  third_party/quiche/src/spdy/core/hpack/hpack_static_table.h
  third_party/quiche/src/spdy/core/http2_frame_decoder_adapter.cc
  third_party/quiche/src/spdy/core/http2_frame_decoder_adapter.h
  third_party/quiche/src/spdy/core/recording_headers_handler.cc
  third_party/quiche/src/spdy/core/recording_headers_handler.h
  third_party/quiche/src/spdy/core/spdy_alt_svc_wire_format.cc
  third_party/quiche/src/spdy/core/spdy_alt_svc_wire_format.h
  third_party/quiche/src/spdy/core/spdy_bitmasks.h
  third_party/quiche/src/spdy/core/spdy_frame_builder.cc
  third_party/quiche/src/spdy/core/spdy_frame_builder.h
  third_party/quiche/src/spdy/core/spdy_framer.cc
  third_party/quiche/src/spdy/core/spdy_framer.h
  third_party/quiche/src/spdy/core/spdy_header_block.cc
  third_party/quiche/src/spdy/core/spdy_header_block.h
  third_party/quiche/src/spdy/core/spdy_header_storage.cc
  third_party/quiche/src/spdy/core/spdy_header_storage.h
  third_party/quiche/src/spdy/core/spdy_headers_handler_interface.h
  third_party/quiche/src/spdy/core/spdy_intrusive_list.h
  third_party/quiche/src/spdy/core/spdy_no_op_visitor.cc
  third_party/quiche/src/spdy/core/spdy_no_op_visitor.h
  third_party/quiche/src/spdy/core/spdy_pinnable_buffer_piece.cc
  third_party/quiche/src/spdy/core/spdy_pinnable_buffer_piece.h
  third_party/quiche/src/spdy/core/spdy_prefixed_buffer_reader.cc
  third_party/quiche/src/spdy/core/spdy_prefixed_buffer_reader.h
  third_party/quiche/src/spdy/core/spdy_protocol.cc
  third_party/quiche/src/spdy/core/spdy_protocol.h
  third_party/quiche/src/spdy/core/spdy_simple_arena.cc
  third_party/quiche/src/spdy/core/spdy_simple_arena.h
  third_party/quiche/src/spdy/core/zero_copy_output_buffer.h
  )

set(quiche_protos
  third_party/quiche/src/quic/core/proto/cached_network_parameters.proto
  third_party/quiche/src/quic/core/proto/crypto_server_config.proto
  third_party/quiche/src/quic/core/proto/source_address_token.proto
  )

set(PROTOBUF_IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/quiche/src)
protobuf_generate_cpp(quiche_protos_SRCS quiche_protos_HDRS ${quiche_protos})

add_library(quiche OBJECT
  ${quiche_SRCS}
  ${quiche_protos_SRCS}
  )

target_include_directories(quiche PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}/third_party/quiche/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/chromium
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}
  third_party/quiche/src/common/platform/default
  third_party/quiche/src
  )

target_link_libraries(quiche PUBLIC
  libprotobuf-lite
  zlib
  absl::synchronization
  absl::stacktrace
  absl::symbolize
  absl::flags
  absl::flags_parse
  absl::flags_marshalling
  )

set(SUPPORT_LIBS quiche ${SUPPORT_LIBS})

if (COMPILER_CLANG)
  set_target_properties(quiche PROPERTIES
    COMPILE_FLAGS "-Wno-missing-prototypes -Wno-implicit-fallthrough -Wno-format-nonliteral")
elseif (COMPILER_GCC)
  set_target_properties(quiche PROPERTIES
    COMPILE_FLAGS "-Wno-missing-declarations -Wno-implicit-fallthrough -Wno-format-nonliteral -Wno-ignored-qualifiers -Wno-unused-variable -Wno-deprecated-copy -Wno-unused-but-set-variable -Wno-strict-overflow")
endif()

# *****************************************************************************************
#           Source code
# *****************************************************************************************

set(files
    src/network.cpp
    src/config/config.cpp
    src/config/config_impl.cpp
    src/core/cipher.cpp
    src/core/hkdf_sha1.cpp
    src/core/hmac_sha1.cpp
    src/core/http_parser.c
    src/core/iobuf.cpp
    src/core/socks4_request_parser.cpp
    src/core/socks5_request_parser.cpp
    src/core/ss_request_parser.cpp
    src/core/stringprintf.cpp
    src/core/rand_util.cpp
    src/core/logging.cpp
    src/core/utils.cpp
    src/core/safe_strerror.cpp
    src/core/debug.cpp
    src/core/string_util.cpp
    src/core/check_op.cpp
    src/core/check.cpp
    src/core/asio.cpp
    src/core/scoped_clear_last_error_win.cpp
    src/core/string_util_constants.cpp
    src/core/ref_counted.cpp
    src/core/escape.cpp
    src/core/string_number_conversions.cpp
    src/core/string_number_conversions_win.cpp
    src/core/utf_offset_string_conversions.cpp
    src/core/utf_string_conversion_utils.cpp
    src/core/base64.cpp
    src/core/hash.cpp
    src/core/unguessable_token.cpp
    src/core/pickle.cpp
    src/core/token.cpp
    src/core/atl_throw.cpp
    src/crypto/aead_base_decrypter.cpp
    src/crypto/aead_base_encrypter.cpp
    src/crypto/aead_evp_decrypter.cpp
    src/crypto/aead_evp_encrypter.cpp
    src/crypto/aead_sodium_decrypter.cpp
    src/crypto/aead_sodium_encrypter.cpp
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
    )

set(hfiles
    src/config/config.hpp
    src/config/config_impl.hpp
    src/config/config_impl_posix.hpp
    src/config/config_impl_windows.hpp
    src/core/cipher.hpp
    src/core/hkdf_sha1.hpp
    src/core/hmac_sha1.hpp
    src/core/http_parser.h
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
    src/core/rand_util.hpp
    src/core/logging.hpp
    src/core/utils.hpp
    src/core/safe_strerror.hpp
    src/core/debug.hpp
    src/core/string_util.hpp
    src/core/check_op.hpp
    src/core/check.hpp
    src/core/asio.hpp
    src/core/atomic_ref_count.hpp
    src/core/gtest_prod_util.hpp
    src/core/ref_counted.hpp
    src/core/thread_annotations.hpp
    src/core/clamped_math.hpp
    src/core/clamped_math_impl.hpp
    src/core/contains.hpp
    src/core/escape.hpp
    src/core/functional/identity.hpp
    src/core/functional/invoke.hpp
    src/core/functional/not_fn.hpp
    src/core/ranges/algorithm.hpp
    src/core/ranges/functional.hpp
    src/core/ranges/ranges.hpp
    src/core/safe_math.hpp
    src/core/string_number_conversions.hpp
    src/core/string_number_conversions_internal.hpp
    src/core/string_number_conversions_win.hpp
    src/core/utf_offset_string_conversions.hpp
    src/core/utf_string_conversion_utils.hpp
    src/core/hash.hpp
    src/core/base64.hpp
    src/core/token.hpp
    src/core/unguessable_token.hpp
    src/core/checked_iterators.hpp
    src/core/contiguous_iterator.hpp
    src/core/cxx20_to_address.hpp
    src/core/pickle.hpp
    src/core/span.hpp
    src/core/token.hpp
    src/core/atl_throw.hpp
    src/core/atl.hpp
    src/crypto/aead_base_decrypter.hpp
    src/crypto/aead_base_encrypter.hpp
    src/crypto/aead_evp_decrypter.hpp
    src/crypto/aead_evp_encrypter.hpp
    src/crypto/aead_sodium_decrypter.hpp
    src/crypto/aead_sodium_encrypter.hpp
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
    )

if (MSVC)
  list(APPEND files src/core/rand_util_win.cpp)
  list(APPEND files src/core/utils_win.cpp)
elseif (APPLE)
  list(APPEND files src/config/config_impl_apple.hpp)
  list(APPEND files src/config/config_impl_apple.mm)
  list(APPEND files src/core/rand_util_posix.cpp)
  list(APPEND files src/core/utils_mac.mm)
  list(APPEND files src/core/foundation_util.mm)
else()
  list(APPEND files src/core/rand_util_posix.cpp)
  list(APPEND files src/core/utils_linux.cpp)
endif()

add_library(yass_base OBJECT
    ${files} ${hfiles}
    )

set_target_properties(yass_base PROPERTIES POSITION_INDEPENDENT_CODE True)
target_include_directories(yass_base PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

if (CLI)
  add_executable(yass_cli
      src/cli/cli.cpp
      src/cli/socks5_connection.cpp
      src/cli/socks5_connection.hpp
      src/cli/socks5_connection_stats.cpp
      src/cli/socks5_connection_stats.hpp
      $<TARGET_OBJECTS:yass_base>
      )

  target_include_directories(yass_cli PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src)

  target_link_libraries(yass_cli PUBLIC
      ${SUPPORT_LIBS}
      )

  if (NOT APPLE AND NOT MSVC)
    install(TARGETS yass_cli DESTINATION bin)
  endif()
endif()

if (SERVER)
  add_executable(yass_server
      src/server/server.cpp
      src/server/ss_connection.cpp
      src/server/ss_connection.hpp
      $<TARGET_OBJECTS:yass_base>
      )

  target_include_directories(yass_server PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src)

  target_link_libraries(yass_server PUBLIC
      ${SUPPORT_LIBS}
      )

  if (NOT APPLE AND NOT MSVC)
    install(TARGETS yass_server DESTINATION bin)
  endif()
endif()

# *****************************************************************************************
#           Source code: GUI part
# *****************************************************************************************

if (GUI)
    set(APP_NAME yass)
    set(SRC_FILES
        src/cli/cli_worker.cpp
        src/cli/socks5_connection.cpp
        src/cli/socks5_connection.hpp
        src/cli/socks5_connection_stats.cpp
        src/cli/socks5_connection_stats.hpp
        $<TARGET_OBJECTS:yass_base>
        )

    if (MSVC)
        set(APP_MSVC_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/yass.manifest")
        configure_file("src/win32/yass.manifest.in" "${APP_MSVC_MANIFEST}" @ONLY)

        set(APP_MSVC_VERSION_RC "${CMAKE_CURRENT_BINARY_DIR}/yass_version.rc")
        configure_file("src/win32/yass_version.rc.in" "${APP_MSVC_VERSION_RC}" @ONLY)
        set(APP_MSVC_ABOUT_DIALOG_RC "${CMAKE_CURRENT_BINARY_DIR}/yass_about.rc")
        configure_file("src/win32/yass_about.rc.in" "${APP_MSVC_ABOUT_DIALOG_RC}" @ONLY)

        list(APPEND SRC_FILES
          ${APP_MSVC_MANIFEST}
          ${APP_MSVC_VERSION_RC}
          ${APP_MSVC_ABOUT_DIALOG_RC}
          src/win32/yass.rc
          src/win32/yass.cpp
          src/win32/yass_frame.cpp
          src/win32/option_dialog.cpp
          src/win32/utils_win.cpp)
    elseif (APPLE)
        set(MAIN_STORYBOARD ${CMAKE_CURRENT_SOURCE_DIR}/src/mac/Base.lproj/Main.storyboard)
        set(ASSET_CATALOG src/mac/Assets.xcassets)
        set(ASSET_CATALOG_ASSETS Assets)
        list(APPEND SRC_FILES
          ${MAIN_STORYBOARD})
        list(APPEND SRC_FILES
          ${ASSET_CATALOG})
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
    else()
        list(APPEND SRC_FILES
          src/gtk/yass.cpp
          src/gtk/yass_window.cpp
          src/gtk/option_dialog.cpp
          src/gtk/utils_gui.cpp)
    endif()

    add_executable(${APP_NAME} WIN32 MACOSX_BUNDLE
        ${SRC_FILES}
        ${GUI_USE_FILE}
        )

    target_include_directories(${APP_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

    target_include_directories(${APP_NAME} SYSTEM PRIVATE
        ${GUI_INCLUDE_DIRS})

    if (MSVC)
      target_include_directories(${APP_NAME} PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/win32)
    elseif(APPLE)
      target_include_directories(${APP_NAME} PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/mac)
    else()
      target_include_directories(${APP_NAME} PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gui)
    endif()

    foreach(GUI_DEFINITION IN LISTS GUI_DEFINITIONS)
        target_compile_definitions(${APP_NAME} PRIVATE ${GUI_DEFINITION})
    endforeach()

    target_compile_options(${APP_NAME} PRIVATE ${GUI_C_CXX_FLAGS})

    link_directories(${GUI_LIBRARY_DIRS})

    target_link_libraries(${APP_NAME} PUBLIC ${GUI_LIBRARIES} ${SUPPORT_LIBS})

    if (MSVC)
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

    if (APPLE)
        get_target_property(_APP_COMPILE_FLAGS ${APP_NAME} COMPILE_FLAGS)
        if (NOT _APP_COMPILE_FLAGS)
          set(_APP_COMPILE_FLAGS "")
        endif()
        set_target_properties(${APP_NAME} PROPERTIES
            COMPILE_FLAGS "${_APP_COMPILE_FLAGS} -fobjc-arc")

        # Handle the Storyboard ourselves
        if(NOT ${CMAKE_GENERATOR} MATCHES "^Xcode.*")
          # Compile the storyboard file with the ibtool.
          add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${IBTOOL}
            --errors
            --warnings
            --notices
            --output-format human-readable-text
            --compile ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app/Contents/Resources/Main.storyboardc
            ${MAIN_STORYBOARD}
            COMMENT "Compiling Main storyboard")
        endif()

        # adds to Copy Bundle Resources build phase
        set_source_files_properties("src/mac/yass.icns" PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources")
        set_source_files_properties("${MAIN_STORYBOARD}" PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources")
        set_source_files_properties("${ASSET_CATALOG}" PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources")

        set(APP_MACOSX_BUNDLE_INFO "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
        configure_file("src/mac/Info.plist.in" "${APP_MACOSX_BUNDLE_INFO}" @ONLY)

        set_target_properties(${APP_NAME} PROPERTIES
            RESOURCE "yass.icns ${CMAKE_CURRENT_SOURCE_DIR}/src/mac/Base.lproj/Main.storyboard"
            MACOSX_BUNDLE_ICON_FILE yass.icns
            MACOSX_BUNDLE_COPYRIGHT "${YASS_APP_COPYRIGHT}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "it.gui.yass"
            MACOSX_BUNDLE_BUNDLE_NAME ${PACKAGE_NAME}
            MACOSX_BUNDLE_INFO_PLIST ${APP_MACOSX_BUNDLE_INFO}
            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
            XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
            XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++"
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
    endif()

    if (NOT APPLE AND NOT MSVC)
      install(TARGETS ${APP_NAME} DESTINATION bin)

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

# *****************************************************************************************
#           Source code: Unit Tests part
# *****************************************************************************************

if (BUILD_TESTS)
  add_subdirectory(third_party/googletest)
  set_target_properties(gtest PROPERTIES
    COMPILE_FLAGS "-Wno-error")
  set_target_properties(gmock PROPERTIES
    COMPILE_FLAGS "-Wno-error")
endif()
