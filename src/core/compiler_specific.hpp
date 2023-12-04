// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */
#ifndef CORE_COMPILER_SPECIFIC_H
#define CORE_COMPILER_SPECIFIC_H

#include <base/compiler_specific.h>
#include <build/build_config.h>

// clang 14 doesn't recognize the newer NOINLINE definitions
#if defined(__clang__) && HAS_ATTRIBUTE(noinline) && ((defined(_BASE_APPLE_CLANG_VER) && _BASE_APPLE_CLANG_VER <= 1500) || (defined(_BASE_CLANG_VER) && _BASE_CLANG_VER <= 1400))
#undef NOINLINE
#define NOINLINE __attribute__((noinline))
#endif

// clang 13 doesn't recognize the newer ALWAYS_INLINE definitions
#if defined(__clang__) && HAS_ATTRIBUTE(always_inline) && ((defined(_BASE_APPLE_CLANG_VER) && _BASE_APPLE_CLANG_VER <= 1400) || (defined(_BASE_CLANG_VER) && _BASE_CLANG_VER <= 1300))
#undef ALWAYS_INLINE
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#endif

#if defined(__apple_build_version__)
// Given AppleClang XX.Y.Z, _BASE_COMPILER_CLANG_BASED is XXYZ (e.g. AppleClang 14.0.3 => 1403)
#  define _BASE_COMPILER_CLANG_BASED
#  define _BASE_APPLE_CLANG_VER (__apple_build_version__ / 10000)
#elif defined(__clang__)
#  define _BASE_COMPILER_CLANG_BASED
#  define _BASE_CLANG_VER (__clang_major__ * 100 + __clang_minor__)
#elif defined(__GNUC__)
#  define _BASE_COMPILER_GCC
#  define _BASE_GCC_VER (__GNUC__ * 100 + __GNUC_MINOR__)
#endif

// Annotate a function indicating the caller must examine the return value.
// Use like:
//   int foo() WARN_UNUSED_RESULT;
// To explicitly ignore a result, see |ignore_result()| in base/macros.h.
#undef WARN_UNUSED_RESULT
#if defined(COMPILER_GCC) || defined(__clang__)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif (defined(_MSVC_LANG) && _MSVC_LANG > 201703) || \
  (defined(__cplusplus) && __cplusplus >= 201703L)
#define WARN_UNUSED_RESULT [[nodiscard]]
#else
#define WARN_UNUSED_RESULT
#endif

// MemorySanitizer annotations.
#if defined(__has_feature)
#  if __has_feature(memory_sanitizer)
#    define MEMORY_SANITIZER 1
#  endif
#endif

#if defined(MEMORY_SANITIZER) && !defined(OS_NACL)
#undef MSAN_UNPOISON
#undef MSAN_CHECK_MEM_IS_INITIALIZED

#include <sanitizer/msan_interface.h>

// Mark a memory region fully initialized.
// Use this to annotate code that deliberately reads uninitialized data, for
// example a GC scavenging root set pointers from the stack.
#define MSAN_UNPOISON(p, size) __msan_unpoison(p, size)

// Check a memory region for initializedness, as if it was being used here.
// If any bits are uninitialized, crash with an MSan report.
// Use this to sanitize data which MSan won't be able to track, e.g. before
// passing data to another process via shared memory.
#define MSAN_CHECK_MEM_IS_INITIALIZED(p, size) \
  __msan_check_mem_is_initialized(p, size)

#define NO_SANITIZE_MEMORY NO_SANITIZE("memory")
#else  // MEMORY_SANITIZER
#define MSAN_UNPOISON(p, size)
#define MSAN_CHECK_MEM_IS_INITIALIZED(p, size)

#define NO_SANITIZE_MEMORY
#endif  // MEMORY_SANITIZER

#if HAS_FEATURE(thread_sanitizer) || __SANITIZE_THREAD__
#define _SANITIZE_THREAD 1
#define THREAD_SANITIZER 1
#endif

// Use nomerge attribute to disable optimization of merging multiple same calls.
#if defined(__clang__) && HAS_ATTRIBUTE(nomerge) && ((defined(_BASE_APPLE_CLANG_VER) && _BASE_APPLE_CLANG_VER >= 1300) || (defined(_BASE_CLANG_VER) && _BASE_CLANG_VER >= 1200))
#undef NOMERGE
#define NOMERGE [[clang::nomerge]]
#else
#define NOMERGE
#endif

// no more defined in code
#if HAS_ATTRIBUTE(noreturn) || (defined(__GNUC__) && !defined(__clang__))
#define NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define NORETURN __declspec(noreturn)
#else
#define NORETURN [[noreturn]]
#endif

// no more defined in code
#if defined(_MSC_VER)
#define MSVC_PUSH_DISABLE_WARNING(n) \
  __pragma(warning(push)) __pragma(warning(disable : n))
#define MSVC_POP_WARNING() __pragma(warning(pop))
#else
#define MSVC_PUSH_DISABLE_WARNING(n)
#define MSVC_POP_WARNING()
#endif

#include <base/dcheck_is_on.h>

#endif  // CORE_COMPILER_SPECIFIC_H
