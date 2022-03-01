project(cxx CXX)

# The OLD behavior of this policy is to only allow GENERATED to be
# visible from the directory scope for which it was set.  The NEW
# behavior on the other hand allows it to be visible from any scope.
#
# Turn on Fix on https://gitlab.kitware.com/cmake/cmake/-/issues/18399
# Introduced by cmake 3.20
if(POLICY CMP0118)
  cmake_policy(SET CMP0118 NEW)
endif()

option(BUILD_SHARED_LIBS "Build with shared libraries." OFF)

# https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?redirectedfrom=MSDN&view=msvc-170
set(CMAKE_MSVC_CRT_LINKAGE "static" CACHE STRING "")

## Enforce C/C++ standard
set(CMAKE_CXX_STANDARD 17)

# This is included by reference in the //build/config/compiler:runtime_library
# config that is applied to all targets. It is here to separate out the logic
# that is specific to libc++. Please see that target for advice on what should
# go in :runtime_library vs. :compiler.
if (BUILD_SHARED_LIBS OR CMAKE_MSVC_CRT_LINKAGE STREQUAL "dynamic")
  set(libcxx_SHARED ON)
else()
  set(libcxx_SHARED OFF)
endif()

set(libcxx_ABI_UNSTABLE OFF)

if (MSVC)
  option(ALLOW_XP "Build with Windows XP support." OFF)
endif()

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

#The system libc++ on Mac doesn't have aligned allocation in C++17.
if (APPLE)
  add_definitions(-D_LIBCPP_HAS_NO_ALIGNED_ALLOCATION)
endif()

message(STATUS "Build with libc++")
if (libcxx_ABI_UNSTABLE)
  set(libcxx_PUBLIC_DEFINITIONS
    ${libcxx_PUBLIC_DEFINITIONS}
    "_LIBCPP_ABI_UNSTABLE")
endif()

set(libcxx_CR "79a2e924d96e2fc1e4b937c42efd08898fa472d7")
if (libcxx_SHARED)
  # When libcxx_is_shared is true, symbols from libc++.so are exported for
  # all DSOs to use.  If the system libc++ gets loaded (indirectly through
  # a system library), then it will conflict with our libc++.so.  Add a
  # custom ABI version if we're building with _LIBCPP_ABI_UNSTABLE to avoid
  # conflicts.
  #
  # Windows doesn't need to set _LIBCPP_ABI_VERSION since there's no system
  # C++ library we could conflict with.
  if (libcxx_ABI_UNSTABLE AND NOT MSVC)
    set(libcxx_PUBLIC_DEFINITIONS
      ${libcxx_PUBLIC_DEFINITIONS}
      "_LIBCPP_ABI_VERSION=${libcxx_CR}")
  endif()
else()
  # Don't leak any symbols on a static build.
  set(libcxx_PUBLIC_DEFINITIONS
    ${libcxx_PUBLIC_DEFINITIONS}
    "_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS")
  if (NOT MSVC)
    set(libcxx_PUBLIC_DEFINITIONS
      ${libcxx_PUBLIC_DEFINITIONS}
      "_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS")
  endif()
endif()

set(libcxx_PUBLIC_DEFINITIONS
  ${libcxx_PUBLIC_DEFINITIONS}
  "_LIBCPP_ENABLE_NODISCARD")

set(libcxx_PUBLIC_INCLUDES
  ${libcxx_PUBLIC_INCLUDES}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

# The Windows component build fails to link with libc++'s debug mode. See
# https://crbug.com/923166#c33, https://crbug.com/923166#c44, and
# https://llvm.org/PR41018.
if (NOT MSVC)
  # libc++ has two levels of debug mode. Setting _LIBCPP_DEBUG to zero
  # enables most assertions. Setting it to one additionally enables iterator
  # debugging. See https://libcxx.llvm.org/docs/DesignDocs/DebugMode.html
  if (${CMAKE_BUILD_TYPE} MATCHES RELWITHDEBINFO)
   set(libcxx_PUBLIC_DEFINITIONS
     ${libcxx_PUBLIC_DEFINITIONS}
     "_LIBCPP_DEBUG=1")
  elseif (${CMAKE_BUILD_TYPE} MATCHES Debug)
   set(libcxx_PUBLIC_DEFINITIONS
     ${libcxx_PUBLIC_DEFINITIONS}
     "_LIBCPP_DEBUG=0")
  endif()
endif()

if (MSVC)
  # Intentionally not using libc++abi on Windows because libc++abi only
  # implements the Itanium C++ ABI, and not the Microsoft ABI which we use on
  # Windows (and we need to use in order to interoperate correctly with COM
  # among other things).
  set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -I ${CMAKE_CURRENT_SOURCE_DIR}/trunk/include"
    )

  # Prevent libc++ from embedding linker flags to try to automatically link
  # against its runtime library. This is unnecessary with our build system,
  # and can also result in build failures if libc++'s name for a library
  # does not match ours.
  set(libcxx_PUBLIC_DEFINITIONS
    ${libcxx_PUBLIC_DEFINITIONS}
    "_LIBCPP_NO_AUTO_LINK")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdinc++ -isystem ${CMAKE_CURRENT_SOURCE_DIR}/trunk/include -isystem ${CMAKE_CURRENT_SOURCE_DIR}/../libc++abi/trunk/include"
    )

  set(libcxx_PUBLIC_DEFINITIONS
    ${libcxx_PUBLIC_DEFINITIONS}
    "CR_LIBCXX_REVISION=${libcxx_CR}")

  # Make sure we don't link against the system libstdc++ or libc++.
  if (COMPILER_CLANG)
    set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -nostdlib++")
    set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -nostdlib++")
  else()
    # Gcc has a built-in abs() definition with default visibility.
    # If it was not disabled, it would conflict with libc++'s abs()
    # with hidden visibility.
    set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -fno-builtin-abs")
    set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fno-builtin-abs")

    set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -nodefaultlibs")
    set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -nodefaultlibs")

    # Unfortunately, there's no way to disable linking against just libc++
    # (gcc doesn't have -notstdlib++:
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83931); -nodefaultlibs
    # removes all of the default libraries, so add back the ones that we need.
    set(libcxx_PUBLIC_LIBRARIES
      ${libcxx_PUBLIC_LIBRARIES}
      c gcc_s m rt
      )
  endif()
endif()

set(libcxx_SOURCES
  trunk/src/algorithm.cpp
  trunk/src/any.cpp
  trunk/src/atomic.cpp
  trunk/src/barrier.cpp
  trunk/src/bind.cpp
  trunk/src/charconv.cpp
  trunk/src/chrono.cpp
  trunk/src/condition_variable.cpp
  trunk/src/condition_variable_destructor.cpp
  trunk/src/debug.cpp
  trunk/src/exception.cpp
  trunk/src/functional.cpp
  trunk/src/future.cpp
  trunk/src/hash.cpp
  trunk/src/ios.cpp
  trunk/src/ios.instantiations.cpp
  trunk/src/iostream.cpp
  trunk/src/locale.cpp
  trunk/src/memory.cpp
  trunk/src/mutex.cpp
  trunk/src/mutex_destructor.cpp
  trunk/src/new.cpp
  trunk/src/optional.cpp
  trunk/src/random.cpp
  trunk/src/random_shuffle.cpp
  trunk/src/regex.cpp
  trunk/src/shared_mutex.cpp
  trunk/src/stdexcept.cpp
  trunk/src/string.cpp
  trunk/src/strstream.cpp
  trunk/src/system_error.cpp
  trunk/src/thread.cpp
  trunk/src/typeinfo.cpp
  trunk/src/utility.cpp
  trunk/src/valarray.cpp
  trunk/src/vector.cpp
)

# GCC 6 series on Debian Stretch doesn't compile
if (NOT COMPILER_GCC OR (COMPILER_GCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.0))
  set(libcxx_SOURCES
    ${libcxx_SOURCES}
    trunk/src/variant.cpp
    )
endif()

set(libcxx_DEFINITIONS "_LIBCPP_BUILDING_LIBRARY")

if (MSVC)
  ## Enable exceptions in the STL
  set(libcxx_DEFINITIONS ${libcxx_DEFINITIONS}
    "_HAS_EXCEPTIONS=1")
endif()

if (MSVC)
  set(libcxx_SOURCES
    ${libcxx_SOURCES}
    trunk/src/support/win32/locale_win32.cpp
    trunk/src/support/win32/support.cpp
    trunk/src/support/win32/thread_win32.cpp
  )

  # Explicitly set version macros to Windows 7 to prevent libc++ from adding a
  # hard dependency on GetSystemTimePreciseAsFileTime, which was introduced in
  # Windows 8.
  if (ALLOW_XP)
    set(libcxx_DEFINITIONS
      ${libcxx_DEFINITIONS}
      "NTDDI_VERSION=NTDDI_WINXP"
      "_WIN32_WINNT=_WIN32_WINNT_WINXP"
      "WINVER=_WIN32_WINNT_WINXP"
      "_WIN32_WINNT_LONGHORN=0x0600"
      "_WIN32_WINNT_WIN7=0x0601"
      "_WIN32_WINNT_WIN8=0x0602"
      "_WIN32_WINNT_WINBLUE=0x0603"
      "_WIN32_WINNT_WINTHRESHOLD=0x0A00"
      "_WIN32_WINNT_WIN10=0x0A00"
    )
  else()
    set(libcxx_DEFINITIONS
      ${libcxx_DEFINITIONS}
      "NTDDI_VERSION=NTDDI_WIN7"
      "_WIN32_WINNT=_WIN32_WINNT_WIN7"
      "WINVER=_WIN32_WINNT_WIN7"
      "_WIN32_WINNT_LONGHORN=0x0600"
      "_WIN32_WINNT_WIN7=0x0601"
      "_WIN32_WINNT_WIN8=0x0602"
      "_WIN32_WINNT_WINBLUE=0x0603"
      "_WIN32_WINNT_WINTHRESHOLD=0x0A00"
      "_WIN32_WINNT_WIN10=0x0A00"
    )
  endif()
endif()

set(libcxx_CFLAGS "-fstrict-aliasing")

if (COMPILER_CLANG)
  set(libcxx_CFLAGS ${libcxx_CFLAGS} "-Wno-missing-prototypes")
elseif (COMPILER_GCC)
  set(libcxx_CFLAGS ${libcxx_CFLAGS} "-Wno-missing-declarations")
endif()

if (MSVC)
  set(libcxx_CFLAGS ${libcxx_CFLAGS}
    # libc++ wants to redefine the macros WIN32_LEAN_AND_MEAN and _CRT_RAND_S in
    # its implementation.
    "-Wno-macro-redefined"
    # We want to use a uniform C++ version across all of chromium, but
    # upstream libc++ requires C++17 so we have to make an exception here.
    "-std:c++17"
    )
else()
  set(libcxx_CFLAGS ${libcxx_CFLAGS}
    "-fPIC"
    "-std=c++17"
    )
endif()

if (NOT libcxx_SHARED)
  add_library(cxx STATIC ${libcxx_SOURCES})
  if (APPLE AND COMPILER_CLANG)
    # We want operator new/delete to be private on Mac, but these functions
    # are implicitly created by the compiler for each translation unit, as
    # specified in the C++ spec 3.7.4p2, which makes them always have default
    # visibility.  This option is needed to force hidden visibility since
    # -fvisibility=hidden doesn't have the desired effect.
    set(libcxx_CFLAGS ${libcxx_CFLAGS}
      "-fvisibility-global-new-delete-hidden"
    )
  else()
    # This resets the visibility to default only for the various
    # flavors of operator new and operator delete.  These symbols
    # are weak and get overriden by Chromium-provided ones, but if
    # these symbols had hidden visibility, this would make the
    # Chromium symbols hidden too because elf visibility rules
    # require that linkers use the least visible form when merging,
    # and if this is hidden, then when we merge it with tcmalloc's
    # operator new, hidden visibility would win. However, tcmalloc
    # needs a visible operator new to also override operator new
    # references from system libraries.
    # TODO(lld): Ask lld for a --force-public-visibility flag or
    # similar to that overrides the default elf merging rules, and
    # make tcmalloc's gn config pass that to all its dependencies,
    # then remove this override here.
    set(libcxx_DEFINITIONS
      ${libcxx_DEFINITIONS}
      "_LIBCPP_OVERRIDABLE_FUNC_VIS=__attribute__((__visibility__(\"default\")))"
    )
  endif()

  if (NOT MSVC)
    set(libcxx_CFLAGS ${libcxx_CFLAGS}
      "-fvisibility-inlines-hidden" "-fvisibility=hidden"
    )
  endif()
else()
  add_library(cxx SHARED ${libcxx_SOURCES})
  if (NOT MSVC)
    set(libcxx_CFLAGS ${libcxx_CFLAGS}
      "-fvisibility=default"
    )
  endif()
endif()

if (NOT APPLE AND (ASAN OR TSAN OR MSAN))
  # In {a,t,m}san configurations, operator new and operator delete will be
  # provided by the sanitizer runtime library.  Since libc++ defines these
  # symbols with weak linkage, and the *san runtime uses strong linkage, it
  # should technically be OK to omit this, but it's added to be explicit.
  set(libcxx_DEFINITIONS
    ${libcxx_DEFINITIONS}
    "_LIBCPP_DISABLE_NEW_DELETE_DEFINITIONS"
  )
endif()

if (NOT MSVC)
  set(libcxx_DEFINITIONS
    ${libcxx_DEFINITIONS}
    "LIBCXX_BUILDING_LIBCXXABI")
  set(libcxxabi_SOURCES
    ../libc++abi/trunk/src/abort_message.cpp
    ../libc++abi/trunk/src/cxa_aux_runtime.cpp
    ../libc++abi/trunk/src/cxa_default_handlers.cpp
    ../libc++abi/trunk/src/cxa_exception.cpp
    ../libc++abi/trunk/src/cxa_exception_storage.cpp
    ../libc++abi/trunk/src/cxa_handlers.cpp
    # This file is supposed to be used in fno-exception builds of
    # libc++abi.  We build lib++/libc++abi with exceptions enabled.
    #trunk/src/cxa_noexception.cpp
    ../libc++abi/trunk/src/cxa_personality.cpp
    ../libc++abi/trunk/src/cxa_vector.cpp
    ../libc++abi/trunk/src/cxa_virtual.cpp
    ../libc++abi/trunk/src/fallback_malloc.cpp
    ../libc++abi/trunk/src/private_typeinfo.cpp
    ../libc++abi/trunk/src/stdlib_exception.cpp
    ../libc++abi/trunk/src/stdlib_stdexcept.cpp
    ../libc++abi/trunk/src/stdlib_typeinfo.cpp
    )
  if (NOT TSAN)
    set(libcxxabi_SOURCES
      ${libcxxabi_SOURCES}
      ../libc++abi/trunk/src/cxa_guard.cpp
    )
  endif()
  if (NOT MSVC AND NOT APPLE)
    set(libcxxabi_SOURCES
      ${libcxxabi_SOURCES}
      ../libc++abi/trunk/src/cxa_thread_atexit.cpp
    )
  endif()
  set(libcxxabi_DEFINITIONS
    ${libcxxabi_DEFINITIONS}
    "LIBCXXABI_SILENT_TERMINATE"
    "_LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS"
    )

  if (NOT libcxx_SHARED)
    add_library(cxxabi STATIC ${libcxxabi_SOURCES})
  else()
    add_library(cxxabi SHARED ${libcxxabi_SOURCES})
  endif()

  foreach(Definition ${libcxxabi_DEFINITIONS})
    target_compile_definitions(cxxabi PRIVATE ${Definition})
  endforeach()

  target_include_directories(cxxabi PRIVATE
    ../libc++
    ../libc++/trunk/src
    ../libc++abi/trunk/include)

  # libc++abi depends on libc++ internals.
  target_include_directories(cxxabi PRIVATE
    ../libc++/trunk/include)

  if (libcxx_SHARED)
    target_compile_options(cxxabi PRIVATE "-fvisibility=default")
  endif()

  target_compile_options(cxxabi PRIVATE "-frtti")
  target_compile_options(cxxabi PRIVATE "-fexceptions")

  target_link_libraries(cxx cxxabi)
endif()

target_compile_definitions(cxx PRIVATE ${libcxx_DEFINITIONS})

target_include_directories(cxx PRIVATE
  ../libc++
  ../libc++/trunk/include)

target_compile_options(cxx PRIVATE ${libcxx_CFLAGS})

if (NOT MSVC AND NOT APPLE AND NOT COMPILER_CLANG)
  target_link_libraries(cxx -Wl,'-Bstatic,-latomic,-Bdynamic')
endif()

if (MSVC)
  target_compile_options(cxx PRIVATE "/GR")
  if (${CMAKE_BUILD_TYPE} MATCHES Debug)
    set(DLL_SUFFIX "d")
  endif()
  if (CMAKE_MSVC_CRT_LINKAGE STREQUAL "dynamic")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEFAULTLIB:msvcprt${DLL_SUFFIX}.lib")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEFAULTLIB:msvcprt${DLL_SUFFIX}.lib")
  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEFAULTLIB:libcpmt${DLL_SUFFIX}.lib")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEFAULTLIB:libcpmt${DLL_SUFFIX}.lib")
  endif()
else()
  target_compile_options(cxx PRIVATE "-frtti")
  target_compile_options(cxx PRIVATE "-fexceptions")
endif()

include_directories(trunk/include)

# export to parent project
set(libcxx_PUBLIC_INCLUDES ${libcxx_PUBLIC_INCLUDES} PARENT_SCOPE)
set(libcxx_PUBLIC_DEFINITIONS ${libcxx_PUBLIC_DEFINITIONS} PARENT_SCOPE)
set(libcxx_PUBLIC_LIBRARIES ${libcxx_PUBLIC_LIBRARIES} PARENT_SCOPE)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} PARENT_SCOPE)
set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} PARENT_SCOPE)
set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} PARENT_SCOPE)

include_directories(${libcxx_PUBLIC_INCLUDES})
foreach(Definition ${libcxx_PUBLIC_DEFINITIONS})
  add_definitions(-D${Definition})
endforeach()
