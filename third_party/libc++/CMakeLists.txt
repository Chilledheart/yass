set(CMAKE_CXX_STANDARD 20)

## Setup compiler informations
if (COMPILER_APPLE_CLANG AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0)
  message(FATAL_ERROR "Requires Apple Clang 15.0 or newer to compile libc++ from source")
endif()

if (COMPILER_CLANG AND NOT COMPILER_APPLE_CLANG AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.0)
  message(FATAL_ERROR "Requires Clang 16.0 or newer to compile libc++ from source")
endif()

if (COMPILER_GCC AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.0)
  message(FATAL_ERROR "Requires gcc 13.0 or newer to compile libc++ from source")
endif()

# This is included by reference in the //build/config/compiler:runtime_library
# config that is applied to all targets. It is here to separate out the logic
# that is specific to libc++. Please see that target for advice on what should
# go in :runtime_library vs. :compiler.
if (BUILD_SHARED_LIBS)
  set(libcxx_SHARED ON)
else()
  set(libcxx_SHARED OFF)
endif()

# experimental, turn off to build libc++ without exceptions
if (MSVC OR APPLE)
  set(libcxx_USE_EXCEPTIONS ON)
else()
  # FIXME workaround with newer lld linkage issue with different exception frames
  # ld.lld: error: relocation R_MIPS_32 cannot be used against local symbol; recompile with -fPIC
  # >>> defined in thinlto-cache/llvmcache-C577B9C572ACF510A97E7F74D80906652031E64C
  # >>> referenced by chrono.cpp
  # >>>               thinlto-cache/llvmcache-C577B9C572ACF510A97E7F74D80906652031E64C:(.eh_frame+0xAC69)
  set(libcxx_USE_EXCEPTIONS OFF)
endif()

# gcc's lto implementation is buggy and cannot find std::terminate if libc++ built with -fexceptions while used with
# -fno-exceptions, setting to OFF
if (COMPILER_GCC AND LTO_FLAVOUR)
  set(libcxx_USE_EXCEPTIONS OFF)
endif()

message(STATUS "Build with libc++")

# *****************************************************************************************
#           Public-specific
# *****************************************************************************************

# replace -stdlib=libc++ with ""
set(CompilerFlags
  CMAKE_CXX_FLAGS
  CMAKE_CXX_FLAGS_DEBUG
  CMAKE_CXX_FLAGS_MINSIZEREL
  CMAKE_CXX_FLAGS_RELEASE
  CMAKE_CXX_FLAGS_RELWITHDEBINFO
  )
foreach(CompilerFlag ${CompilerFlags})
  string(REPLACE "-stdlib=libc++" "" ${CompilerFlag} "${${CompilerFlag}}")
endforeach()

set(libcxx_CR "edb1e0a15de69d2d8fb62ff87f5d52987ad849b8")
# Fixed libc++ configuration macros are in
# buildtools/third_party/libc++/__config_site. This config only has defines
# that vary depending on gn args, and non-define flags.

if (NOT libcxx_SHARED)
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

# libc++ has two levels of additional checking:
# 1. _LIBCPP_ENABLE_ASSERTIONS enables assertions for bounds checking.
#    We always enable this in __config_site, in all build configurations.
# 2. _LIBCPP_ENABLE_DEBUG_MODE enables iterator debugging and other
#    expensive checks. Enable these only if enable_iterator_debugging is on.
if (${CMAKE_BUILD_TYPE} MATCHES Debug)
 set(libcxx_PUBLIC_DEFINITIONS
   ${libcxx_PUBLIC_DEFINITIONS}
   "_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG")
else()
 set(libcxx_PUBLIC_DEFINITIONS
   ${libcxx_PUBLIC_DEFINITIONS}
   "_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE")
endif()

set(libcxx_PUBLIC_DEFINITIONS
  ${libcxx_PUBLIC_DEFINITIONS}
  "CR_LIBCXX_REVISION=${libcxx_CR}")

# Normally, this would be defined in the `runtime_library` config but NaCl
# saigo libc++ does not use the custom hermetic libc++. Unfortunately, there
# isn't really a better config to add this define for the define to
# consistently apply in both Chromium and non-Chromium code *and* non-NaCl
# and NaCl code.
#
# TODO(https://crbug.com/702997): Move this back to the `runtime_library`
# config when NaCl is removed.

if (USE_MUSL)
  set(libcxx_PUBLIC_DEFINITIONS
    ${libcxx_PUBLIC_DEFINITIONS}
    "_LIBCPP_HAS_MUSL_LIBC")
endif()

if (MSVC)
  # Intentionally not using libc++abi on Windows because libc++abi only
  # implements the Itanium C++ ABI, and not the Microsoft ABI which we use on
  # Windows (and we need to use in order to interoperate correctly with COM
  # among other things).
  set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -I ${CMAKE_CURRENT_SOURCE_DIR} -I ${CMAKE_CURRENT_SOURCE_DIR}/trunk/include"
    )

else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdinc++ -isystem ${CMAKE_CURRENT_SOURCE_DIR} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/trunk/include -isystem ${CMAKE_CURRENT_SOURCE_DIR}/../libc++abi/trunk/include"
    )

  # Make sure we don't link against the system libstdc++ or libc++.
  if (COMPILER_CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0 AND NOT (MINGW AND MINGW_MSVCRT100))
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -nostdlib++")
    if (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-L${CMAKE_CURRENT_SOURCE_DIR}/freebsd")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-L${CMAKE_CURRENT_SOURCE_DIR}/freebsd")
    endif()
  else()
    # Gcc has a built-in abs() definition with default visibility.
    # If it was not disabled, it would conflict with libc++'s abs()
    # with hidden visibility.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin-abs")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-abs")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nodefaultlibs")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -nodefaultlibs")

    # Unfortunately, there's no way to disable linking against just libc++
    # (gcc doesn't have -notstdlib++:
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83931); -nodefaultlibs
    # removes all of the default libraries, so add back the ones that we need.
    if (UNIX)
      if (APPLE)
        set(libcxx_PUBLIC_LIBRARIES
          ${libcxx_PUBLIC_LIBRARIES}
          c m gcc_eh gcc System
          )
      elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        set(libcxx_PUBLIC_LIBRARIES
          ${libcxx_PUBLIC_LIBRARIES}
          pthread c gcc_s m rt
          )
      else()
        set(libcxx_PUBLIC_LIBRARIES
          ${libcxx_PUBLIC_LIBRARIES}
          c m gcc_s rt
          )
      endif()
    elseif (MINGW AND MINGW_MSVCRT100)
      if (MINGW_COMPILER_RT)
        set(libcxx_PUBLIC_LIBRARIES
          ${libcxx_PUBLIC_LIBRARIES}
           ${CMAKE_SYSROOT}/lib/libmingw32.a ${MINGW_COMPILER_RT} unwind moldname mingwex msvcr100
           advapi32 shell32 user32 kernel32
           ${CMAKE_SYSROOT}/lib/libmingw32.a ${MINGW_COMPILER_RT} unwind moldname mingwex msvcr100 kernel32
          )
      else()
        set(libcxx_PUBLIC_LIBRARIES
          ${libcxx_PUBLIC_LIBRARIES}
           mingw32 gcc_eh gcc moldname mingwex msvcr100
           advapi32 shell32 user32 kernel32
           mingw32 gcc_eh gcc moldname mingwex msvcr100 kernel32
          )
      endif()
    endif()
  endif()
endif()

# *****************************************************************************************
#           Private-specific
# *****************************************************************************************

set(libcxx_SOURCES
  trunk/src/algorithm.cpp
  trunk/src/any.cpp
  trunk/src/atomic.cpp
  trunk/src/barrier.cpp
  trunk/src/bind.cpp
  trunk/src/call_once.cpp
  trunk/src/charconv.cpp
  trunk/src/chrono.cpp
  trunk/src/condition_variable.cpp
  trunk/src/condition_variable_destructor.cpp
  trunk/src/error_category.cpp
  trunk/src/exception.cpp
  trunk/src/functional.cpp
  trunk/src/future.cpp
  trunk/src/hash.cpp
  trunk/src/ios.cpp
  trunk/src/ios.instantiations.cpp
  trunk/src/iostream.cpp
  trunk/src/legacy_pointer_safety.cpp
  trunk/src/locale.cpp
  trunk/src/memory.cpp
  trunk/src/mutex.cpp
  trunk/src/mutex_destructor.cpp
  trunk/src/new_handler.cpp
  trunk/src/new_helpers.cpp
  trunk/src/optional.cpp
  trunk/src/random.cpp
  trunk/src/random_shuffle.cpp
  trunk/src/regex.cpp
  trunk/src/ryu/d2fixed.cpp
  trunk/src/ryu/d2s.cpp
  trunk/src/ryu/f2s.cpp
  trunk/src/shared_mutex.cpp
  trunk/src/stdexcept.cpp
  trunk/src/string.cpp
  trunk/src/strstream.cpp
  trunk/src/system_error.cpp
  trunk/src/thread.cpp
  trunk/src/typeinfo.cpp
  trunk/src/valarray.cpp
  trunk/src/vector.cpp
  trunk/src/verbose_abort.cpp
  trunk/src/filesystem/path.cpp
  trunk/src/filesystem/filesystem_error.cpp
)

# TODO move to base
if (NOT (WIN32 AND ALLOW_XP AND OS_X86))
  set(libcxx_SOURCES
    ${libcxx_SOURCES}
    trunk/src/filesystem/directory_iterator.cpp
    trunk/src/filesystem/operations.cpp
    )
endif()

if (APPLE OR (NOT ASAN AND NOT TSAN AND NOT MSAN))
  # In {a,t,m}san configurations, operator new and operator delete will be
  # provided by the sanitizer runtime library.  Since libc++ defines these
  # symbols with weak linkage, and the *san runtime uses strong linkage, it
  # should technically be OK to include this file, but it's removed to be
  # explicit.
  set(libcxx_SOURCES
    ${libcxx_SOURCES}
    trunk/src/new.cpp)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(libcxx_SOURCES
    ${libcxx_SOURCES}
    trunk/src/filesystem/directory_entry.cpp
    trunk/src/filesystem/filesystem_clock.cpp
  )
endif()

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

if (WIN32)
  set(libcxx_SOURCES
    ${libcxx_SOURCES}
    trunk/src/support/win32/locale_win32.cpp
    trunk/src/support/win32/support.cpp
    trunk/src/support/win32/thread_win32.cpp
  )
  # hacks, override posix thread model tls implementation
  if (MINGW AND MINGW_WORKAROUND)
    set(libcxx_SOURCES
      ${libcxx_SOURCES}
      emutls.cpp
      )
    set_source_files_properties(emutls.cpp
      PROPERTIES
      COMPILE_FLAGS "-fno-builtin")
  endif()

  # turn on win32 thread model manually
  if (MINGW)
    set(libcxx_PUBLIC_DEFINITIONS
      ${libcxx_PUBLIC_DEFINITIONS}
      "_LIBCPP_HAS_THREAD_API_WIN32"
    )
  endif()

  # Explicitly set version macros to Windows 7 to prevent libc++ from adding a
  # hard dependency on GetSystemTimePreciseAsFileTime, which was introduced in
  # Windows 8.
  if (ALLOW_XP AND OS_X86)
    set(libcxx_DEFINITIONS
      ${libcxx_DEFINITIONS}
      "NTDDI_VERSION=NTDDI_WINXP"
    )
  elseif(OS_AARCH64)
    set(libcxx_DEFINITIONS
      ${libcxx_DEFINITIONS}
      "NTDDI_VERSION=NTDDI_WIN10"
    )
  else()
    set(libcxx_DEFINITIONS
      ${libcxx_DEFINITIONS}
      "NTDDI_VERSION=NTDDI_WIN7"
    )
  endif()
endif()

if (ANDROID)
  set(libcxx_SOURCES
    ${libcxx_SOURCES}
    sync_synchronize.S
    )
endif()

set(libcxx_CFLAGS "-fstrict-aliasing")

if (COMPILER_CLANG)
  set(libcxx_CFLAGS ${libcxx_CFLAGS} "-Wno-missing-prototypes")
elseif (COMPILER_GCC)
  set(libcxx_CFLAGS ${libcxx_CFLAGS} "-Wno-missing-declarations")
endif()

if (COMPILER_GCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-alloc-size-larger-than")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-alloc-size-larger-than")
endif()

if (COMPILER_GCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11.0)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-stringop-overread -Wno-array-bounds")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overread -Wno-array-bounds")
endif()

if (COMPILER_GCC)
  set(libcxx_CFLAGS ${libcxx_CFLAGS} "-Wno-attributes")
endif()

if (WIN32)
  set(libcxx_CFLAGS ${libcxx_CFLAGS}
    # libc++ wants to redefine the macros WIN32_LEAN_AND_MEAN and _CRT_RAND_S in
    # its implementation.
    "-Wno-macro-redefined"
    )
else()
  set(libcxx_CFLAGS ${libcxx_CFLAGS}
    "-fPIC"
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
    # FIXME remove this once apple clang is updated to TOT
    if (COMPILER_APPLE_CLANG)
      set(libcxx_CFLAGS ${libcxx_CFLAGS}
        "-fvisibility-global-new-delete-hidden"
      )
    else()
      set(libcxx_CFLAGS ${libcxx_CFLAGS}
        "-fvisibility-global-new-delete=force-hidden"
      )
    endif()
  else ()
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
    # mingw's binutils ld linker cannot find weak symbols
    # https://sourceware.org/bugzilla/show_bug.cgi?id=9687
    if (MINGW AND COMPILER_GCC)
      set(libcxx_DEFINITIONS
        ${libcxx_DEFINITIONS}
        "_LIBCPP_WEAK=_LIBCPP_OVERRIDABLE_FUNC_VIS"
        "_LIBCXXABI_WEAK=_LIBCPP_OVERRIDABLE_FUNC_VIS"
      )
    endif()
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

if (USE_LTO_CMAKE)
  set_property(TARGET cxx
    PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
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

if (NOT ${CMAKE_BUILD_TYPE} MATCHES Debug)
  set(libcxx_DEFINITIONS
    ${libcxx_DEFINITIONS}
    "_LIBCPP_AVAILABILITY_CUSTOM_VERBOSE_ABORT_PROVIDED=1")
endif()
if (NOT MSVC)
  set(libcxx_DEFINITIONS
    ${libcxx_DEFINITIONS}
    "LIBCXX_BUILDING_LIBCXXABI")
  set(libcxxabi_SOURCES
    # C++ABI files
    ../libc++abi/trunk/src/abort_message.cpp
    ../libc++abi/trunk/src/cxa_aux_runtime.cpp
    ../libc++abi/trunk/src/cxa_default_handlers.cpp
    ../libc++abi/trunk/src/cxa_exception_storage.cpp
    ../libc++abi/trunk/src/cxa_handlers.cpp
    ../libc++abi/trunk/src/cxa_vector.cpp
    ../libc++abi/trunk/src/cxa_virtual.cpp
    # C++ STL files
    ../libc++abi/trunk/src/stdlib_exception.cpp
    ../libc++abi/trunk/src/stdlib_stdexcept.cpp
    ../libc++abi/trunk/src/stdlib_typeinfo.cpp
    # Internal files
    ../libc++abi/trunk/src/fallback_malloc.cpp
    ../libc++abi/trunk/src/private_typeinfo.cpp
    )
  if (libcxx_USE_EXCEPTIONS)
    set(libcxxabi_SOURCES
      ${libcxxabi_SOURCES}
      ../libc++abi/trunk/src/cxa_exception.cpp
      ../libc++abi/trunk/src/cxa_personality.cpp
      )
  else()
    set(libcxxabi_SOURCES
      ${libcxxabi_SOURCES}
      # This file is supposed to be used in fno-exception builds of
      # libc++abi.  We build lib++/libc++abi with exceptions enabled.
      ../libc++abi/trunk/src/cxa_noexception.cpp
      )
  endif()
  if (NOT TSAN)
    set(libcxxabi_SOURCES
      ${libcxxabi_SOURCES}
      ../libc++abi/trunk/src/cxa_guard.cpp
    )
  endif()
  if (NOT ANDROID)
    set(libcxxabi_SOURCES
      ${libcxxabi_SOURCES}
      ../libc++abi/trunk/src/cxa_demangle.cpp
    )
  endif()
  if (UNIX AND NOT APPLE)
    set(libcxxabi_SOURCES
      ${libcxxabi_SOURCES}
      ../libc++abi/trunk/src/cxa_thread_atexit.cpp
    )
  elseif (MINGW AND (COMPILER_GCC OR MINGW_WORKAROUND))
    # mingw gcc doesn't support weak symbol usage in above source code
    # and triggers SIGSEGV/access violence in running
    set(libcxxabi_SOURCES
      ${libcxxabi_SOURCES}
      mingw_cxa_thread_atexit.cpp
    )

    set(libcxxabi_SOURCES
      ${libcxxabi_SOURCES}
      mingw_dso_handle.cpp
    )
  endif()
  set(libcxxabi_DEFINITIONS
    ${libcxxabi_DEFINITIONS}
    "LIBCXXABI_SILENT_TERMINATE"
    )

  if (ANDROID)
    set(libcxxabi_DEFINITIONS
      ${libcxxabi_DEFINITIONS}
      "HAVE___CXA_THREAD_ATEXIT_IMPL"
      )
  endif()

  if (NOT libcxx_SHARED)
    add_library(cxxabi STATIC ${libcxxabi_SOURCES})
  else()
    add_library(cxxabi SHARED ${libcxxabi_SOURCES})
  endif()

  target_compile_definitions(cxxabi PRIVATE ${libcxxabi_DEFINITIONS})
  target_compile_definitions(cxxabi PRIVATE ${libcxx_DEFINITIONS})
  target_compile_definitions(cxxabi PRIVATE ${libcxx_PUBLIC_DEFINITIONS})
  target_compile_options(cxxabi PRIVATE ${libcxx_CFLAGS})

  if (USE_LTO_CMAKE)
    set_property(TARGET cxxabi
      PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()

  target_include_directories(cxxabi PRIVATE
    ../libc++
    ../libc++/trunk/src
    ../libc++abi/trunk/include
    ../libc++abi/trunk/src)

  # unwind.h under /usr/include is outdated
  # there are four unwind.h under freebsd actually.. pick up llvm's libunwind header in priority
  # https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=261397
  if (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_include_directories(cxxabi PRIVATE /usr/src/contrib/llvm-project/libunwind/include)
  endif()

  # libc++abi depends on libc++ internals.
  target_include_directories(cxxabi PRIVATE
    ../libc++/trunk/include)

  if (libcxx_SHARED)
    target_compile_options(cxxabi PRIVATE "-fvisibility=default")
  endif()

  target_compile_options(cxxabi PRIVATE "-frtti")
  if (libcxx_USE_EXCEPTIONS)
    target_compile_options(cxxabi PRIVATE "-fexceptions")
  else()
    target_compile_options(cxxabi PRIVATE "-fno-exceptions")
  endif()

  target_link_libraries(cxx cxxabi)
endif()

if (ANDROID)
  set(libunwind_SOURCES
    # C++ sources
    ../libunwind/trunk/src/Unwind-EHABI.cpp
    ../libunwind/trunk/src/libunwind.cpp
    # C sources
    ../libunwind/trunk/src/Unwind-sjlj.c
    ../libunwind/trunk/src/UnwindLevel1-gcc-ext.c
    ../libunwind/trunk/src/UnwindLevel1.c
    # ASM sources
    ../libunwind/trunk/src/UnwindRegistersRestore.S
    ../libunwind/trunk/src/UnwindRegistersSave.S
  )
  if (NOT libcxx_SHARED)
    add_library(unwind STATIC ${libunwind_SOURCES})
  else()
    add_library(unwind SHARED ${libunwind_SOURCES})
  endif()

  target_include_directories(unwind PRIVATE
    ../libunwind/trunk/include)

  target_compile_definitions(unwind PRIVATE ${libcxx_DEFINITIONS})
  target_compile_definitions(unwind PRIVATE ${libcxx_PUBLIC_DEFINITIONS})
  target_compile_options(unwind PRIVATE ${libcxx_CFLAGS})

  target_include_directories(unwind PRIVATE
    ../libc++
    ../libc++/trunk/src
    ../libc++abi/trunk/include
    ../libc++abi/trunk/src)

  target_compile_definitions(unwind PRIVATE _LIBUNWIND_IS_NATIVE_ONLY)
  target_compile_definitions(unwind PRIVATE _LIBUNWIND_DISABLE_VISIBILITY_ANNOTATIONS)

  target_compile_options(unwind PRIVATE "-fstrict-aliasing")
  target_compile_options(unwind PRIVATE "-fPIC")
  # ValueAsBitPattern in Unwind-EHABI.cpp is only used on Debug builds.
  target_compile_options(unwind PRIVATE "-Wno-unused-function")
  # libunwind expects to be compiled with unwind tables so it can
  # unwind its own frames.
  target_compile_options(unwind PRIVATE "-funwind-tables")

  target_compile_options(unwind PRIVATE "-frtti")
  if (libcxx_USE_EXCEPTIONS)
    target_compile_options(unwind PRIVATE "-fexceptions")
  else()
    target_compile_options(unwind PRIVATE "-fno-exceptions")
  endif()

  target_link_libraries(cxx unwind)
endif()

target_compile_definitions(cxx PRIVATE ${libcxx_DEFINITIONS})
target_compile_definitions(cxx PRIVATE ${libcxx_PUBLIC_DEFINITIONS})

target_include_directories(cxx PRIVATE
  ../libc++
  ../libc++/trunk/src
  ../libc++/trunk/include)

target_compile_options(cxx PRIVATE ${libcxx_CFLAGS})

# We need to link with libatomic on systems that do not have builtin atomics, or
# don't have builtin support for 8 byte atomics
set(libcxx_LINK_LIBATOMIC FALSE)
if (NOT MSVC)
  include(CheckCSourceCompiles)
  set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
  set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES} ${libcxx_PUBLIC_LIBRARIES}")
  if (COMPILER_CLANG)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror=atomic-alignment")
  endif()
  check_c_source_compiles("
    #include <stdint.h>
    int main(int argc, char** argv) {
      int64_t val;
      __atomic_store_n(&val, 0LL, __ATOMIC_RELEASE);
      return __atomic_load_n(&val, __ATOMIC_ACQUIRE);
    }
  " libcxx_HAVE_BUILTIN_ATOMICS)
  if (NOT libcxx_HAVE_BUILTIN_ATOMICS)
    set(libcxx_LINK_LIBATOMIC TRUE)
  endif()
  set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})

  if (libcxx_LINK_LIBATOMIC)
    target_link_libraries(cxx atomic)
  endif()
endif()

if (MSVC)
  target_compile_options(cxx PRIVATE "/GR")
  if (${CMAKE_BUILD_TYPE} MATCHES Debug)
    set(DLL_SUFFIX "d")
  endif()
  if (MSVC_CRT_LINKAGE STREQUAL "dynamic")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEFAULTLIB:msvcprt${DLL_SUFFIX}.lib")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEFAULTLIB:msvcprt${DLL_SUFFIX}.lib")
  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEFAULTLIB:libcpmt${DLL_SUFFIX}.lib")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEFAULTLIB:libcpmt${DLL_SUFFIX}.lib")
  endif()
else()
  target_compile_options(cxx PRIVATE "-frtti")
  if (libcxx_USE_EXCEPTIONS)
    target_compile_options(cxx PRIVATE "-fexceptions")
  else()
    target_compile_options(cxx PRIVATE "-fno-exceptions")
  endif()
endif()

include_directories(trunk/include)

set(libcxx_PUBLIC_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/__config_site
  ${CMAKE_CURRENT_SOURCE_DIR}/__assertion_handler)

# export to parent project
set(libcxx_PUBLIC_INCLUDES ${libcxx_PUBLIC_INCLUDES} PARENT_SCOPE)
set(libcxx_PUBLIC_DEFINITIONS ${libcxx_PUBLIC_DEFINITIONS} PARENT_SCOPE)
set(libcxx_PUBLIC_LIBRARIES ${libcxx_PUBLIC_LIBRARIES} PARENT_SCOPE)

foreach(IncludeHeader ${libcxx_PUBLIC_INCLUDES})
  if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FI ${IncludeHeader}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include ${IncludeHeader}")
  endif()
endforeach()

foreach(Definition ${libcxx_PUBLIC_DEFINITIONS})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${Definition}")
endforeach()

if (NOT WIN32)
  foreach(Library ${libcxx_PUBLIC_LIBRARIES})
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-l${Library}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-l${Library}")
  endforeach()
endif()

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} PARENT_SCOPE)
set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} PARENT_SCOPE)
set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} PARENT_SCOPE)

if (NOT MSVC)
  export(
    TARGETS cxxabi
    FILE cxxabi.cmake
  )
endif()
if (ANDROID)
  export(
    TARGETS unwind
    FILE unwind.cmake
  )
endif()
export(
  TARGETS cxx
  FILE cxx.cmake
)
