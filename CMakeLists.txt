cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

if (CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(FATAL_ERROR "In source-tree build not support")
endif()

set(PACKAGE_NAME            "yass")
set(PACKAGE_FULL_NAME       "YetAnotherShadowSocket")
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
# https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?redirectedfrom=MSDN&view=msvc-170
set(CMAKE_MSVC_CRT_LINKAGE "dynamic" CACHE STRING "")
if (VCPKG_CRT_LINKAGE AND NOT VCPKG_CRT_LINKAGE STREQUAL CMAKE_MSVC_CRT_LINKAGE)
  message(FATAL_ERROR "CRT LINKAGE not matched in configuration")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:${CMAKE_MSVC_CRT_LINKAGE},dynamic>:DLL>" CACHE STRING "")

# Defer enabling C and CXX languages.
project(${PACKAGE_NAME} NONE)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Set variables used in configure_file
set(YASS_APP_NAME ${PACKAGE_NAME})
set(YASS_APP_VERSION ${PACKAGE_VERSION})
set(YASS_APP_VERSION_MAJOR ${PACKAGE_VERSION_MAJOR})
set(YASS_APP_VERSION_MINOR ${PACKAGE_VERSION_MINOR})
set(YASS_APP_VERSION_PATCH ${PACKAGE_VERSION_PATCH})
set(YASS_APP_VERSION_TWEAK ${PACKAGE_VERSION_TWEAK})
set(YASS_APP_FULL_VERSION ${PACKAGE_VERSION}.${PACKAGE_VERSION_TWEAK})

if(MSVC)
  # On Windows, prefer cl over gcc if both are available. By default most of
  # the CMake generators prefer gcc, even on Windows.
  set(CMAKE_GENERATOR_CC cl)

  # new in cmake 3.10
  # https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ARCHITECTURE_ID.html
  # not used in ABI-detection due to the cmake issue https://gitlab.kitware.com/cmake/cmake/-/issues/17702
  set(CMAKE_C_COMPILER_ARCHITECTURE_ID "${VCPKG_TARGET_ARCHITECTURE}" CACHE STRING "")
  set(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "${VCPKG_TARGET_ARCHITECTURE}" CACHE STRING "")
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

## Export build compilation database if possible
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

## Enforce C/C++ standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)

if (MSVC)
  set(CMAKE_CXX_STANDARD 17)
endif()

option(CLI "Build with cli." ON)
option(SERVER "Build with server." ON)
option(GUI "Build against GUI." OFF)
option(GUI_SANDBOX "Build against GUI sandbox (MAC Only)." OFF)

# *****************************************************************************************
#           Third Party Libraries
# *****************************************************************************************

set(boringssl_DIR "${CMAKE_SOURCE_DIR}/third_party/boringssl" CACHE STRING "path to boringssl root directory")

include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

check_cxx_source_compiles ("
__thread int tls;
int main() { }
" HAVE_GCC_TLS)

check_cxx_source_compiles ("
__declspec(thread) int tls;
int main() { }
" HAVE_MSVC_TLS)

check_cxx_source_compiles ("
thread_local int tls;
int main() { }
" HAVE_CXX11_TLS)

if (WITH_TLS)
  # Cygwin does not support the thread attribute. Don't bother.
  if (HAVE_GCC_TLS)
    add_definitions(-DHAVE_GCC_TLS)
  elseif (HAVE_MSVC_TLS)
    add_definitions(-DHAVE_MSVC_TLS)
  elseif (HAVE_CXX11_TLS)
    add_definitions(-DHAVE_CXX11_TLS)
  endif()
endif()

check_cxx_source_compiles ("
#include <type_traits>
std::aligned_storage<sizeof(char), alignof(char)>::type data;
int main() { }
" HAVE_ALIGNED_STORAGE)
if (HAVE_ALIGNED_STORAGE)
  add_definitions(-DHAVE_ALIGNED_STORAGE)
endif()

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

    # boringssl
    find_library(BORINGSSL_CRYPTO_RELEASE_LIB crypto
      PATHS ${boringssl_DIR}/${MSVC_C_ARCHITECTURE_ID}-${CMAKE_MSVC_CRT_LINKAGE}
            ${boringssl_DIR}/${MSVC_C_ARCHITECTURE_ID}
            ${boringssl_DIR}/
            ${boringssl_DIR}/build/crypto/Release
            ${boringssl_DIR}/build/Release/crypto
      REQUIRED
      NO_DEFAULT_PATH)
    find_library(BORINGSSL_CRYPTO_DEBUG_LIB crypto
      PATHS ${boringssl_DIR}/${MSVC_C_ARCHITECTURE_ID}-${CMAKE_MSVC_CRT_LINKAGE}/debug
            ${boringssl_DIR}/${MSVC_C_ARCHITECTURE_ID}/debug
            ${boringssl_DIR}/debug
            ${boringssl_DIR}/build/crypto/Debug
            ${boringssl_DIR}/build/Debug/crypto
      REQUIRED
      NO_DEFAULT_PATH)
    set(BORINGSSL_CRYPTO_LIB
      debug ${BORINGSSL_CRYPTO_DEBUG_LIB}
      optimized ${BORINGSSL_CRYPTO_RELEASE_LIB}
      )

    set(CMAKE_REQUIRED_INCLUDES
      ${boringssl_DIR}/include
      ${CMAKE_REQUIRED_INCLUDES})
    check_include_file(openssl/is_boringssl.h BORINGSSL_CRYPTO_INCLUDE)
    add_definitions(-DHAVE_BORINGSSL)
    include_directories(BEFORE ${boringssl_DIR}/include SYSTEM)
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${BORINGSSL_CRYPTO_LIB})
    message(STATUS "Build with boringssl")

    find_library(ShLwApi_LIB ShLwApi REQUIRED)
    find_library(Ws2_32_LIB Ws2_32 REQUIRED)
    find_library(WinMM_LIB WinMM REQUIRED)
    find_library(Gdi32_LIB Gdi32 REQUIRED)
    find_library(Comdlg32_LIB Comdlg32 REQUIRED)
    find_library(Shell32_LIB Shell32 REQUIRED)
    find_library(Ole32_LIB Ole32 REQUIRED)
    find_library(Winspool_LIB Winspool REQUIRED)

    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${ShLwApi_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Ws2_32_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${WinMM_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Gdi32_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Comdlg32_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Shell32_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Ole32_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Winspool_LIB})

    if (CMAKE_MSVC_CRT_LINKAGE STREQUAL "static")
      message(STATUS "Build against static crt")
      # replace /MD with /MT
      set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
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
else()
    if (CMAKE_FIND_ROOT_PATH)
        foreach(ROOT_PATH ${CMAKE_FIND_ROOT_PATH})
            include_directories(${ROOT_PATH}/include SYSTEM)
            link_directories(${ROOT_PATH}/lib)
        endforeach()
    endif()
    if (UNIX AND NOT APPLE)
        include_directories(/usr/include/jsoncpp SYSTEM)
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

    if (NOT APPLE)
      find_library(JSONCPP_LIB jsoncpp)
      add_definitions(-DJSON_USE_EXCEPTION=0)
      set(SYSTEM_LIBS ${SYSTEM_LIBS} ${JSONCPP_LIB})
    endif()

    # boringssl
    find_library(BORINGSSL_CRYPTO_LIB crypto
      PATHS
        ${boringssl_DIR}
        ${boringssl_DIR}/build
        ${boringssl_DIR}/build/crypto
      REQUIRED
      NO_DEFAULT_PATH)

    set(CMAKE_REQUIRED_INCLUDES
        ${boringssl_DIR}/include
        ${CMAKE_REQUIRED_INCLUDES})
    check_include_file(openssl/is_boringssl.h BORINGSSL_CRYPTO_INCLUDE)
    add_definitions(-DHAVE_BORINGSSL)
    include_directories(BEFORE ${boringssl_DIR}/include SYSTEM)
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${BORINGSSL_CRYPTO_LIB})

    # linux-syscall-support
    if (UNIX AND NOT APPLE)
      include_directories(third_party/lss)
    endif()
    if (APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -weak_framework AppKit")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -weak_framework AppKit")
    endif()
endif()

set(SUPPORT_LIBS
    ${SYSTEM_LIBS}
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
    absl::failure_signal_handler
    absl::time
    absl::optional
    )

if (MSVC AND GUI)
  message(STATUS "Build with Platform $ENV{Platform}")
  if (CMAKE_MSVC_CRT_LINKAGE STREQUAL "dynamic")
    message(STATUS "Build against MFC unicode dll")
    add_definitions(-D_AFXDLL)
    # for shared MFC library
    set(CMAKE_MFC_UNICODE_FLAG 2)
  else()
    message(STATUS "Build against MFC unicode static library")
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
  if (CMAKE_MSVC_CRT_LINKAGE STREQUAL "static")
    set(GUI_LIBRARIES uafxcw$<$<CONFIG:Debug>:d> libcmt$<$<CONFIG:Debug>:d>)
  else()
    set(GUI_LIBRARIES "")
  endif()
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
  pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
  set(GUI_USE_FILE "")
  set(GUI_INCLUDE_DIRS ${GTKMM_INCLUDE_DIRS})
  set(GUI_DEFINITIONS "-DGTKMM_DISABLE_DEPRECATED=1 -DGIOMM_DISABLE_DEPRECATED=1 ${GTKMM_CFLAGS_OTHER}")
  set(GUI_C_CXX_FLAGS ${GTKMM_CFLAGS})
  set(GUI_LIBRARY_DIRS ${GTKMM_LIBRARY_DIRS})
  set(GUI_LIBRARIES ${GTKMM_LIBRARIES})
endif()

# abseil-cpp
set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory(third_party/abseil-cpp)
include_directories(third_party/abseil-cpp SYSTEM)

# asio
add_definitions(-DASIO_STANDALONE)
add_definitions(-DASIO_NO_TYPEID)
add_definitions(-DASIO_NO_EXCEPTIONS)
include_directories(third_party/asio/asio/include SYSTEM)

# *****************************************************************************************
#           Compiler warning flags
# *****************************************************************************************
if (COMPILER_GCC OR COMPILER_CLANG OR ENABLE_CLANG_TIDY)
  # Note clang-cl is odd and sets both CLANG and MSVC. We base our configuration
  # primarily on our normal Clang one.
  set(C_CXX_FLAGS "-Wformat=2 -Wsign-compare -Wmissing-field-initializers -Wwrite-strings -Wvla -Wshadow")

  ## doesn't enable clang-tidy's -Werror which is annoying
  if (NOT ENABLE_CLANG_TIDY)
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -Werror")
  endif()

  if (MSVC)
    # clang-cl sets different default warnings than clang. It also treats -Wall
    # as -Weverything, to match MSVC. Instead -W3 is the alias for -Wall.
    # See http://llvm.org/viewvc/llvm-project?view=revision&revision=319116
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -W3 -Wno-unused-parameter -fmsc-version=1900")
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

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS} -Wmissing-prototypes -Wold-style-definition -Wstrict-prototypes")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_CXX_FLAGS} -Wmissing-declarations")

  if(NOT MSVC)
    if(APPLE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
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

  # TODO not compatible with LTO now
  # https://clang.llvm.org/docs/LTOVisibility.html
  # please note it afffects windows platform as well
  if (NOT APPLE AND NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
  endif()

  # ignore unknown prgramas, works for both of gcc and clang
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")

  # ignore complains from clang-cl and mfc/afx
  if (COMPILER_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-local-typedef")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedef")
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
      "C5246" # ALT/MFC suppresion starts here
      )
  set(MSVC_LEVEL4_WARNINGS_LIST
      # See https://connect.microsoft.com/VisualStudio/feedback/details/1217660/warning-c4265-when-using-functional-header
      "C4265" # class has virtual functions, but destructor is not virtual
      )
  string(REPLACE "C" " -wd" MSVC_DISABLED_WARNINGS_STR
                            ${MSVC_DISABLED_WARNINGS_LIST})
  string(REPLACE "C" " -w4" MSVC_LEVEL4_WARNINGS_STR
                            ${MSVC_LEVEL4_WARNINGS_LIST})
  set(CMAKE_C_FLAGS   "-utf-8 -Wall -WX ${MSVC_DISABLED_WARNINGS_STR} ${MSVC_LEVEL4_WARNINGS_STR}")
  set(CMAKE_CXX_FLAGS "-utf-8 -Wall -WX ${MSVC_DISABLED_WARNINGS_STR} ${MSVC_LEVEL4_WARNINGS_STR}")
  # Currently, STL is not cleanly compiled with /Wall
  # VS 2017 and higher supports STL-only warning suppressions.
  # https://github.com/microsoft/STL/blob/main/stl/inc/yvals_core.h#L479
  # https://devblogs.microsoft.com/cppblog/stl-fixes-in-vs-2017-rtm/
  add_definitions(-D_STL_EXTRA_DISABLED_WARNINGS=5220\ 5204\ 4355\ 4774\ 4987\ 4582\ 4583)
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
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections")

    if (APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -dead_strip")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -dead_strip")
    else()
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
    endif()

    ## adopt LTO
    if (APPLE)
        set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -flto -fwhole-program-vtables")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -fwhole-program-vtables")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -flto -fwhole-program-vtables")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto -fwhole-program-vtables")
    endif()

    ## definitions
    add_definitions(-D_FILE_OFFSET_BITS=64)
    add_definitions(-D__STDC_CONSTANT_MACROS)
    add_definitions(-D__STDC_FORMAT_MACROS)

    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -pthread -pipe")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -pipe")

    if (NOT APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,noexecstack -Wl,-z,relro")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,noexecstack -Wl,-z,relro")
    endif()

    ## disable rtti, wxWidgets modal requires this
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

    ## disable exceptions
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

    ## not omit stack frame pointer for release
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")

    ## When compiling Objective-C, warns if a method is used whose
    ## availability is newer than the deployment target.
    if (APPLE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunguarded-availability")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunguarded-availability")
    endif()
else()
    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/GR" "/GR-" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()

    ## disabled rtti
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

    ## C4530, The code uses C++ exception handling,
    ## but /EHsc wasn't included in the compiler options
    ## https://docs.microsoft.com/en-us/cpp/build/reference/eh-exception-handling-model?view=msvc-170
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

    ## Optimization flags
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Zc:inline /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zc:inline /DNDEBUG")

    if (NOT COMPILER_CLANG)
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF")
      set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF")
    endif()

    ## Generate debug info
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zi")

    # Tell linker to include symbol data
    if (NOT COMPILER_CLANG)
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO /DEBUG /OPT:ICF")
      set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO /DEBUG /OPT:ICF")
    endif()

    ## Disable LTCG for static libraries because this setting introduces ABI incompatibility between minor compiler versions
    if (NOT VCPKG_LIBRARY_LINKAGE STREQUAL "static")
      set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /GL")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
      set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
      set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    endif()

    ## #define WINVER 0x0A00
    ## #define _WIN32_WINNT 0x0A00
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

    ## definitions
    add_definitions(-D_HAS_EXCEPTIONS=0)
    add_definitions(-DNOMINMAX)
    add_definitions(-DUNICODE)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DWINVER=0x0601)
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_SCL_SECURE_NO_DEPRECATE)
    add_definitions(-D_UNICODE)
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-DXP_OS2)
    add_definitions(-D_WIN32)
    add_definitions(-DWIN32)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)

    ## not omit stack frame pointer
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Oy-")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oy-")

    ## supress some warnings
    if (COMPILER_CLANG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-microsoft-exception-spec")
    endif(COMPILER_CLANG)
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
    src/core/scoped_clear_last_error_win.cpp
    src/core/string_util_constants.cpp
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

  set_target_properties(yass_cli PROPERTIES EXCLUDE_FROM_ALL TRUE)

  target_include_directories(yass_cli PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src)

  target_link_libraries(yass_cli PUBLIC
      ${SUPPORT_LIBS}
      )
endif()

if (SERVER)
  add_executable(yass_server
      src/server/server.cpp
      src/server/ss_connection.cpp
      src/server/ss_connection.hpp
      $<TARGET_OBJECTS:yass_base>
      )

  set_target_properties(yass_server PROPERTIES EXCLUDE_FROM_ALL TRUE)

  target_include_directories(yass_server PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src)

  target_link_libraries(yass_server PUBLIC
      ${SUPPORT_LIBS}
      )
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
        configure_file("src/win32/yass.manifest.in" "${APP_MSVC_MANIFEST}")

        list(APPEND SRC_FILES
          ${APP_MSVC_MANIFEST}
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
          src/mac/network.icns)

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
          src/gui/yass.cpp
          src/gui/yass_window.cpp
          src/gui/option_dialog.cpp
          src/gui/utils_gui.cpp)
    endif()

    add_executable(${APP_NAME} WIN32 MACOSX_BUNDLE
        ${SRC_FILES}
        ${GUI_USE_FILE}
        )

    target_include_directories(${APP_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

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

    target_link_directories(${APP_NAME} PRIVATE ${GUI_LIBRARY_DIRS})

    target_link_libraries(${APP_NAME} PUBLIC ${SUPPORT_LIBS} ${GUI_LIBRARIES}
        )

    if (MSVC)
        set_target_properties(${APP_NAME} PROPERTIES
            COMPILE_PDB_NAME ${APP_NAME}
            COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
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
        set_source_files_properties("src/mac/network.icns" PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources")
        set_source_files_properties("${MAIN_STORYBOARD}" PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources")
        set_source_files_properties("${ASSET_CATALOG}" PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources")

        set_target_properties(${APP_NAME} PROPERTIES
            RESOURCE "network.icns ${CMAKE_CURRENT_SOURCE_DIR}/src/mac/Base.lproj/Main.storyboard"
            MACOSX_BUNDLE_ICON_FILE network.icns
            MACOSX_BUNDLE_COPYRIGHT "Copyright ${PACKAGE_FULL_NAME}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "it.yass.gui"
            MACOSX_BUNDLE_BUNDLE_NAME ${PACKAGE_NAME}
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/src/mac/Info.plist
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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/yass.desktop
        DESTINATION share/applications)

      install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/network.png
        DESTINATION share/pixmaps)

      install(DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/icons/16x16
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/icons/22x22
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/icons/24x24
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/icons/32x32
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/icons/48x48
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/icons/128x128
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/icons/256x256
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/icons/512x512
        DESTINATION share/icons/hicolor)

      ## gtkmm requires it in treeview, oops
      string(REPLACE "-fno-exceptions" "-fexceptions" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()
endif()

## distribute related
find_program(GIT_EXECUTABLE NAMES git git.exe git.cmd)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git AND NOT (${CMAKE_BUILD_TYPE} MATCHES Release))
  execute_process(COMMAND
    ${GIT_EXECUTABLE} rev-list HEAD --count
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE YASS_APP_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE )
  add_definitions(-DYASS_APP_REVISION=${YASS_APP_REVISION})
endif()

## requires gnu tar and sed, oops
set(ARCHIVE_NAME ${YASS_APP_NAME}-${YASS_APP_VERSION})
add_custom_target(dist
  COMMAND ${GIT_EXECUTABLE} ls-files --recurse-submodules |
        tar caf ${CMAKE_CURRENT_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz
        --xform='s,^\./,${ARCHIVE_NAME}/,' -T -
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
