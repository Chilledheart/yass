// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */
#ifndef CORE_COMPILER_SPECIFIC_H
#define CORE_COMPILER_SPECIFIC_H

#include <base/compiler_specific.h>
#include <build/build_config.h>

// clang 14 doesn't recognize the newer NOINLINE definitions
#if defined(__clang__) && HAS_ATTRIBUTE(noinline) &&                      \
    ((defined(_BASE_APPLE_CLANG_VER) && _BASE_APPLE_CLANG_VER <= 1500) || \
     (defined(_BASE_CLANG_VER) && _BASE_CLANG_VER <= 1400))
#undef NOINLINE
#define NOINLINE __attribute__((noinline))
#endif

// clang 13 doesn't recognize the newer ALWAYS_INLINE definitions
#if defined(__clang__) && HAS_ATTRIBUTE(always_inline) &&                 \
    ((defined(_BASE_APPLE_CLANG_VER) && _BASE_APPLE_CLANG_VER <= 1400) || \
     (defined(_BASE_CLANG_VER) && _BASE_CLANG_VER <= 1300))
#undef ALWAYS_INLINE
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#endif

#if defined(__apple_build_version__)
// Given AppleClang XX.Y.Z, _BASE_COMPILER_CLANG_BASED is XXYZ (e.g. AppleClang 14.0.3 => 1403)
#define _BASE_COMPILER_CLANG_BASED
#define _BASE_APPLE_CLANG_VER (__apple_build_version__ / 10000)
#elif defined(__clang__)
#define _BASE_COMPILER_CLANG_BASED
#define _BASE_CLANG_VER (__clang_major__ * 100 + __clang_minor__)
#elif defined(__GNUC__)
#define _BASE_COMPILER_GCC
#define _BASE_GCC_VER (__GNUC__ * 100 + __GNUC_MINOR__)
#endif

// MemorySanitizer annotations.
#if defined(MEMORY_SANITIZER)
#define NO_SANITIZE_MEMORY NO_SANITIZE("memory")
#else  // MEMORY_SANITIZER
#define NO_SANITIZE_MEMORY
#endif  // MEMORY_SANITIZER

// Use nomerge attribute to disable optimization of merging multiple same calls.
#if defined(__clang__) && HAS_ATTRIBUTE(nomerge) &&                       \
    ((defined(_BASE_APPLE_CLANG_VER) && _BASE_APPLE_CLANG_VER >= 1300) || \
     (defined(_BASE_CLANG_VER) && _BASE_CLANG_VER >= 1200))
#undef NOMERGE
#define NOMERGE [[clang::nomerge]]
#else
#define NOMERGE
#endif

#endif  // CORE_COMPILER_SPECIFIC_H
