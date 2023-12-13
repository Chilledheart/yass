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

#endif  // CORE_COMPILER_SPECIFIC_H
