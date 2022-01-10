cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(PACKAGE_NAME      "YetAnotherShadowSocket")

# Defer enabling C and CXX languages.
project(${PACKAGE_NAME} NONE)

if(MSVC)
  # On Windows, prefer cl over gcc if both are available. By default most of
  # the CMake generators prefer gcc, even on Windows.
  set(CMAKE_GENERATOR_CC cl)
endif()

enable_language(C)
enable_language(CXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(HAS_CLANG TRUE)
else()
    set(HAS_CLANG FALSE)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(HAS_GCC TRUE)
else()
    set(HAS_GCC FALSE)
endif()

## Export build compilation database if possible
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

## Enfore C/C++ standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)

if (MSVC)
  set(CMAKE_CXX_STANDARD 17)
endif()

# *****************************************************************************************
#           Basic Setups
# *****************************************************************************************

if (NOT MSVC)
    ## warnings
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

    if (HAS_CLANG)
      set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Werror -Wno-unused-parameter")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-unused-parameter -Wno-inconsistent-missing-override")
    endif()

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
    ## Set Source and Excutable character sets to UTF-8
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

    ## warnings
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} /W4 /wd4099 /wd4100 /wd4127 /wd4244 /wd4267 /wd4505 /wd4838 /wd4996")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /wd4099 /wd4100 /wd4127 /wd4244 /wd4267 /wd4505 /wd4838 /wd4996")

    ## warnings structure was padded due to alignment specifier
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} /wd4324")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4324")

    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/W3" "/W4" ${CompilerFlag} "${${CompilerFlag}}")
      string(REPLACE "/GR" "/GR-" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()

    ## disabled rtti
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

    ## C4530, The code uses C++ exception handling,
    ## but /EHsc wasn't included in the compiler options
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

    ## Optimization flags
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Zc:inline /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zc:inline /DNDEBUG")

    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF")
      set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF")
    endif()

    ## Generate debug info
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zi")

    # Tell linker to include symbol data
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
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
    if (HAS_CLANG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-microsoft-exception-spec")
    endif(HAS_CLANG)

    if (HAS_CLANG)
        set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} /WX")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
    endif()
endif()

# *****************************************************************************************
#           Debug facilities
# *****************************************************************************************

if (UNIX AND ${HAS_CLANG} AND
    CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fcolor-diagnostics")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
endif()

if (UNIX AND ${HAS_GCC} AND
    CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()

if(MSAN)
  if(NOT HAS_CLANG)
    message(FATAL_ERROR "Cannot enable MSAN unless using Clang")
  endif()

  if(ASAN)
    message(FATAL_ERROR "ASAN and MSAN are mutually exclusive")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
  set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
endif()

if(ASAN)
  if(NOT HAS_CLANG)
    message(FATAL_ERROR "Cannot enable ASAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
endif()

if(TSAN)
  if(NOT HAS_CLANG)
    message(FATAL_ERROR "Cannot enable TSAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
endif()

if(UBSAN)
  if(NOT HAS_CLANG)
    message(FATAL_ERROR "Cannot enable UBSAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
endif()

if(GCOV)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

# *****************************************************************************************
#           Support Libraries
# *****************************************************************************************

option(SODIUM "Build against libsodium." ON)
option(BORINGSSL "Build against boringssl." OFF)
set(boringssl_DIR "${CMAKE_SOURCE_DIR}/../boringssl" CACHE STRING "path to boringssl root directory")

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

check_cxx_source_compiles ("
#include <type_traits>
std::aligned_storage<sizeof(char), alignof(char)>::type data;
int main() { }
" HAVE_ALIGNED_STORAGE)

if (WITH_TLS)
  # Cygwin does not support the thread attribute. Don't bother.
  if (HAVE_GCC_TLS)
    set (THREAD_LOCAL_STORAGE "__thread")
  elseif (HAVE_MSVC_TLS)
    set (THREAD_LOCAL_STORAGE "__declspec(thread)")
  elseif (HAVE_CXX11_TLS)
    set (THREAD_LOCAL_STORAGE thread_local)
  endif()
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

    # libsodium
    find_library(SODIUM_RELEASE_LIB libsodium
      PATHS ${VCPKG_STATIC_LIB_PATH} NO_DEFAULT_PATH)
    find_library(SODIUM_DEBUG_LIB libsodium
      PATHS ${VCPKG_STATIC_LIB_DEBUG_PATH} NO_DEFAULT_PATH)
    set(SODIUM_LIB
      debug ${SODIUM_DEBUG_LIB}
      optimized ${SODIUM_RELEASE_LIB}
      )
    check_include_file(sodium.h SODIUM_INCLUDE)
    if (SODIUM AND SODIUM_LIB AND SODIUM_INCLUDE)
      add_definitions(-DHAVE_LIBSODIUM)
      add_definitions(-DSODIUM_STATIC)
      set(SYSTEM_LIBS ${SYSTEM_LIBS} ${SODIUM_LIB})
      message(STATUS "Build with libsodium")
    endif()

    # boringssl
    find_library(BORINGSSL_CRYPTO_LIB crypto
      PATHS ${boringssl_DIR}
      NO_DEFAULT_PATH)

    find_library(BORINGSSL_CRYPTO_RELEASE_LIB crypto
      PATHS ${boringssl_DIR}
            ${boringssl_DIR}/build/crypto/Release
            ${boringssl_DIR}/build/Release/crypto
       NO_DEFAULT_PATH)
    find_library(BORINGSSL_CRYPTO_DEBUG_LIB crypto
      PATHS ${boringssl_DIR}/debug
            ${boringssl_DIR}/build/crypto/Debug
            ${boringssl_DIR}/build/Debug/crypto
       NO_DEFAULT_PATH)
    set(BORINGSSL_CRYPTO_LIB
      debug ${BORINGSSL_CRYPTO_DEBUG_LIB}
      optimized ${BORINGSSL_CRYPTO_RELEASE_LIB}
      )

    set(CMAKE_REQUIRED_INCLUDES
      ${boringssl_DIR}/include
      ${CMAKE_REQUIRED_INCLUDES})
    check_include_file(openssl/is_boringssl.h BORINGSSL_CRYPTO_INCLUDE)
    if (BORINGSSL AND BORINGSSL_CRYPTO_LIB AND BORINGSSL_CRYPTO_INCLUDE)
      add_definitions(-DHAVE_BORINGSSL)
      include_directories(BEFORE ${boringssl_DIR}/include SYSTEM)
      set(SYSTEM_LIBS ${SYSTEM_LIBS} ${BORINGSSL_CRYPTO_LIB})
      message(STATUS "Build with boringssl")
    else()
      message(STATUS "Build without boringssl")
    endif()

    find_library(ShLwApi_LIB ShLwApi)
    find_library(Ws2_32_LIB Ws2_32)
    find_library(WinMM_LIB WinMM)
    find_library(Gdi32_LIB Gdi32)
    find_library(Comdlg32_LIB Comdlg32)
    find_library(Shell32_LIB Shell32)
    find_library(Ole32_LIB Ole32)
    find_library(Winspool_LIB Winspool)

    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${ShLwApi_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Ws2_32_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${WinMM_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Gdi32_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Comdlg32_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Shell32_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Ole32_LIB})
    set(SYSTEM_LIBS ${SYSTEM_LIBS} ${Winspool_LIB})

    if (VCPKG_CRT_LINKAGE STREQUAL "static")
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
    endif()

    # disable precompiled head files for clang
    if (HAS_CLANG)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Y-")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Y-")
    endif()
else()
    # include extra paths
    if (APPLE)
        include_directories(/opt/local/include /usr/local/include SYSTEM)
        set(CMAKE_REQUIRED_INCLUDES
            /opt/local/include
            /usr/local/include
            ${CMAKE_REQUIRED_INCLUDES})
        link_directories(/opt/local/lib /usr/local/lib)
    endif()
    if (UNIX)
        include_directories(/opt/local/include SYSTEM)
        link_directories(/opt/local/lib)
    endif()
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

    find_library(SODIUM_LIB sodium)
    check_include_file(sodium.h SODIUM_INCLUDE)
    if (SODIUM AND SODIUM_LIB AND SODIUM_INCLUDE)
      add_definitions(-DHAVE_LIBSODIUM)
      set(SYSTEM_LIBS ${SYSTEM_LIBS} ${SODIUM_LIB})
    endif()

    find_library(JSONCPP_LIB jsoncpp)
    add_definitions(-DJSON_USE_EXCEPTION=0)

    # boringssl
    find_library(BORINGSSL_CRYPTO_LIB crypto
      PATHS ${boringssl_DIR}/build/crypto
      NO_DEFAULT_PATH)

    set(CMAKE_REQUIRED_INCLUDES
        ${boringssl_DIR}/include
        ${CMAKE_REQUIRED_INCLUDES})
    check_include_file(openssl/is_boringssl.h BORINGSSL_CRYPTO_INCLUDE)
    if (BORINGSSL AND BORINGSSL_CRYPTO_LIB AND BORINGSSL_CRYPTO_INCLUDE)
        add_definitions(-DHAVE_BORINGSSL)
        include_directories(BEFORE ${boringssl_DIR}/include SYSTEM)
        set(SYSTEM_LIBS ${SYSTEM_LIBS} ${BORINGSSL_CRYPTO_LIB})
    endif()

    # linux-syscall-support
    if (UNIX AND NOT APPLE)
      include_directories(third_party/lss)
    endif()
endif()

set(SUPPORT_LIBS
    ${JSONCPP_LIB}
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
    absl::strings
    absl::failure_signal_handler
    absl::time
    absl::optional
    )

if (NOT MSAN AND NOT ASAN AND NOT TSAN AND NOT UBSAN)
    if (NOT APPLE AND NOT MSVC)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,defs -Wl,--as-needed")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,defs -Wl,--as-needed")
    endif()
    set(SUPPORT_LIBS
        ${SUPPORT_LIBS})
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
if (HAS_CLANG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-inconsistent-missing-override -Wno-unused-private-field")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override -Wno-unused-private-field")
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
option(CLI "Build with cli." ON)
option(SERVER "Build with server." ON)

set(files
    src/network.cpp
    src/config/config.cpp
    src/config/config_impl.cpp
    src/core/cipher.cpp
    src/core/hkdf_sha1.cpp
    src/core/hmac_sha1.cpp
    src/core/http_parser.c
    src/core/md5.cc
    src/core/modp_b64.cc
    src/core/sha1.cc
    src/core/iobuf.cpp
    src/core/socks4_request_parser.cpp
    src/core/socks5_request_parser.cpp
    src/core/ss_request_parser.cpp
    src/core/stringprintf.cpp
    src/core/rand_util.cpp
    src/core/logging.cpp
    src/crypto/aead_base_decrypter.cpp
    src/crypto/aead_base_encrypter.cpp
    src/crypto/aead_evp_decrypter.cpp
    src/crypto/aead_evp_encrypter.cpp
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
    src/core/md5.h
    src/core/modp_b64.h
    src/core/modp_b64_data.h
    src/core/sha1.h
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
    src/crypto/aead_base_decrypter.hpp
    src/crypto/aead_base_encrypter.hpp
    src/crypto/aead_evp_decrypter.hpp
    src/crypto/aead_evp_encrypter.hpp
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
else()
  list(APPEND files src/core/rand_util_posix.cpp)
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

  target_include_directories(yass_server PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src)

  target_link_libraries(yass_server PUBLIC
      ${SUPPORT_LIBS}
      )
endif()

# *****************************************************************************************
#           Source code: extra part
# *****************************************************************************************

option(GUI "Build against GUI." OFF)

# change wxWidgets_CONFIG_EXECUTABLE
if (APPLE OR UNIX)
    #set(wxWidgets_CONFIG_EXECUTABLE
    find_program(wxWidgets_CONFIG_EXECUTABLE
      wx-config
      PATHS /opt/wxWidgets/bin
            /opt/local/Library/Frameworks/wxWidgets.framework/Versions/wxWidgets/3.1/bin
            /opt/local/Library/Frameworks/wxWidgets.framework/Versions/wxWidgets/3.0/bin
    )
endif()

if (MSVC)
  message(STATUS "Build with Platform $ENV{Platform}")

  set(wxWidgets_ROOT_DIR ${VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET})
  set(wxWidgets_LIB_DIR ${VCPKG_STATIC_LIB_PATH})
  set(wxWidgets_LIB_DEBUG_DIR ${VCPKG_STATIC_LIB_DEBUG_PATH})
  set(wxWidgets_INCLUDE_DIRS
      ${wxWidgets_ROOT_DIR}/include
      ${wxWidgets_ROOT_DIR}/include/wx/msw)

  if ($ENV{Platform} MATCHES "x64")
    # find manifest if any
    file(GLOB wxWidgets_MANIFESTS "${wxWidgets_ROOT_DIR}/include/wx/msw/amd64_dpi_aware_pmv2.manifest")
  elseif ($ENV{Platform} MATCHES "x86")
    file(GLOB wxWidgets_MANIFESTS "${wxWidgets_ROOT_DIR}/include/wx/msw/wx_dpi_aware_pmv2.manifest")
  endif()

  # find jpeg library
  find_library(JPEG_RELEASE_LIB jpeg
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(JPEG_DEBUG_LIB jpeg
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(JPEG_LIB
    debug ${JPEG_DEBUG_LIB}
    optimized ${JPEG_RELEASE_LIB}
    )

  # find libpng16 library
  find_library(PNG16_RELEASE_LIB libpng16
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(PNG16_DEBUG_LIB libpng16d
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(PNG16_LIB
    debug ${PNG16_DEBUG_LIB}
    optimized ${PNG16_RELEASE_LIB}
    )

  # find lzma library
  find_library(LZMA_RELEASE_LIB lzma
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(LZMA_DEBUG_LIB lzmad
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(LZMA_LIB
    debug ${LZMA_DEBUG_LIB}
    optimized ${LZMA_RELEASE_LIB}
    )

  # find tiff library
  find_library(TIFF_RELEASE_LIB tiff
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(TIFF_DEBUG_LIB tiffd
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(TIFF_LIB
    debug ${TIFF_DEBUG_LIB}
    optimized ${TIFF_RELEASE_LIB}
    )

  # find turbojpeg library
  find_library(TURBOJPEG_RELEASE_LIB turbojpeg
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(TURBOJPEG_DEBUG_LIB turbojpeg
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(TURBOJPEG_LIB
    debug ${TURBOJPEG_DEBUG_LIB}
    optimized ${TURBOJPEG_RELEASE_LIB}
    )

  # find zlib library
  find_library(ZLIB_RELEASE_LIB zlib
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(ZLIB_DEBUG_LIB zlibd
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(ZLIB_LIB
    debug ${ZLIB_DEBUG_LIB}
    optimized ${ZLIB_RELEASE_LIB}
    )

  # find wxregex library
  find_library(WX_REGEX_RELEASE_LIB wxregexu
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(WX_REGEX_DEBUG_LIB wxregexud
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(WX_REGEX_LIB
    debug ${WX_REGEX_DEBUG_LIB}
    optimized ${WX_REGEX_RELEASE_LIB}
    )

  # find wxbase library
  find_library(WX_BASE_RELEASE_LIB wxbase31u
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(WX_BASE_DEBUG_LIB wxbase31ud
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(WX_BASE_LIB
    debug ${WX_BASE_DEBUG_LIB}
    optimized ${WX_BASE_RELEASE_LIB}
    )

  # find wxcore library
  find_library(WX_CORE_RELEASE_LIB wxmsw31u_core
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(WX_CORE_DEBUG_LIB wxmsw31ud_core
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(WX_CORE_LIB
    debug ${WX_CORE_DEBUG_LIB}
    optimized ${WX_CORE_RELEASE_LIB}
    )

  find_library(COMCTL32_LIB COMCTL32)
  find_library(RPCRT4_LIB RPCRT4)
  find_library(UUID_LIB UUID)

  set(wxWidgets_LIBRARIES
      ${JPEG_LIB}
      ${PNG16_LIB}
      ${LZMA_LIB}
      ${TIFF_LIB}
      ${TURBOJPEG_LIB}
      ${ZLIB_LIB}
      ${COMCTL32_LIB}
      ${RPCRT4_LIB}
      ${UUID_LIB}
      ${WX_REGEX_LIB}
      ${WX_BASE_LIB}
      ${WX_CORE_LIB})

  set(wxWidgets_DEFINITIONS
      wxUSE_GUI
      wxUSE_UNICODE
      __WXMSW__
      )
  set(wxWidgets_FOUND TRUE)
else()
  find_package(wxWidgets COMPONENTS core base)
endif()

if (GUI AND wxWidgets_FOUND)
    set(APP_NAME yass)
    set(SRC_FILES
        src/gui/yass.cpp
        src/gui/yass_frame.cpp
        src/gui/yass_logging.cpp
        src/gui/option_dialog.cpp
        src/gui/panels.cpp
        src/gui/worker.cpp
        src/gui/utils.cpp
        src/cli/socks5_connection.cpp
        src/cli/socks5_connection.hpp
        src/cli/socks5_connection_stats.cpp
        src/cli/socks5_connection_stats.hpp
        $<TARGET_OBJECTS:yass_base>
        )

    if (MSVC)
        list(APPEND SRC_FILES yass.rc)
        # But wxWidgets looked different.
        foreach(manifest ${wxWidgets_MANIFESTS})
          string(REPLACE "\\" "/" manifest_win ${manifest})
          list(APPEND SRC_FILES ${manifest_win})
        endforeach()
        list(APPEND SRC_FILES src/gui/utils_win.cpp)
    endif()

    if (APPLE)
        list(APPEND SRC_FILES wxmac.icns)
        list(APPEND SRC_FILES src/gui/utils_mac.mm)
    endif()

    if (UNIX AND NOT APPLE)
        # for Linux, BSD, Solaris, Minix
        list(APPEND SRC_FILES src/gui/utils_linux.cpp)
    endif()

    add_executable(${APP_NAME} WIN32 MACOSX_BUNDLE
        ${SRC_FILES}
        ${wxWidgets_USE_FILE}
        )

    target_include_directories(${APP_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

    target_include_directories(${APP_NAME} SYSTEM PRIVATE
        ${wxWidgets_INCLUDE_DIRS})

    target_link_libraries(${APP_NAME} PUBLIC
        ${SUPPORT_LIBS}
        ${wxWidgets_LIBRARIES}
        )

    target_compile_options(${APP_NAME} PRIVATE ${wxWidgets_CXX_FLAGS})
    foreach(DEF IN LISTS wxWidgets_DEFINITIONS)
        target_compile_definitions(${APP_NAME} PRIVATE ${DEF})
    endforeach()

    target_link_directories(${APP_NAME} PRIVATE ${wxWidgets_LIBRARY_DIRS})

    if (MSVC)
        set_target_properties(${APP_NAME} PROPERTIES
            COMPILE_PDB_NAME ${APP_NAME}
            COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
        )
    endif()

    if (APPLE)
        set_target_properties(${APP_NAME} PROPERTIES
            RESOURCE "wxmac.icns"
            MACOSX_BUNDLE_ICON_FILE wxmac.icns
            MACOSX_BUNDLE_COPYRIGHT "Copyright ${PACKAGE_NAME}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "it.yass.gui"
            MACOSX_BUNDLE_BUNDLE_NAME ${PACKAGE_NAME}
          )
    endif()
endif()
