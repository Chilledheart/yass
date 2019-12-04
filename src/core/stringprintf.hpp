//
// stringprintf.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef H_STRINGPRINTF
#define H_STRINGPRINTF

#include <string>

/* compiler_specific.h */
// Annotate a function indicating the caller must examine the return value.
// Use like:
//   int foo() WARN_UNUSED_RESULT;
// To explicitly ignore a result, see |ignore_result()| in base/macros.h.
#undef WARN_UNUSED_RESULT
#if defined(__GNUC__) || defined(__clang__)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define WARN_UNUSED_RESULT
#endif

// Tell the compiler a function is using a printf-style format string.
// |format_param| is the one-based index of the format string parameter;
// |dots_param| is the one-based index of the "..." parameter.
// For v*printf functions (which take a va_list), pass 0 for dots_param.
// (This is undocumented but matches what the system C headers do.)
// For member functions, the implicit this parameter counts as index 1.
#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_FORMAT(format_param, dots_param) \
    __attribute__((format(printf, format_param, dots_param)))
#else
#define PRINTF_FORMAT(format_param, dots_param)
#endif

// WPRINTF_FORMAT is the same, but for wide format strings.
// This doesn't appear to yet be implemented in any compiler.
// See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=38308 .
#define WPRINTF_FORMAT(format_param, dots_param)
// If available, it would look like:
//   __attribute__((format(wprintf, format_param, dots_param)))

// Return a C++ string given printf-like input.
std::string StringPrintf(const char* format, ...)
    PRINTF_FORMAT(1, 2) WARN_UNUSED_RESULT;
#if defined(_WIN32)
std::wstring StringPrintf(const wchar_t* format, ...)
    WPRINTF_FORMAT(1, 2) WARN_UNUSED_RESULT;
#endif

// Return a C++ string given vprintf-like input.
std::string StringPrintV(const char* format, va_list ap)
    PRINTF_FORMAT(1, 0) WARN_UNUSED_RESULT;

// Store result into a supplied string and return it.
const std::string& SStringPrintf(std::string* dst,
                                             const char* format,
                                             ...) PRINTF_FORMAT(2, 3);
#if defined(_WIN32)
const std::wstring& SStringPrintf(std::wstring* dst,
                                              const wchar_t* format,
                                              ...) WPRINTF_FORMAT(2, 3);
#endif

// Append result to a supplied string.
void StringAppendF(std::string* dst, const char* format, ...)
    PRINTF_FORMAT(2, 3);
#if defined(_WIN32)
void StringAppendF(std::wstring* dst, const wchar_t* format, ...)
    WPRINTF_FORMAT(2, 3);
#endif

// Lower-level routine that takes a va_list and appends to a specified
// string.  All other routines are just convenience wrappers around it.
void StringAppendV(std::string* dst, const char* format, va_list ap)
    PRINTF_FORMAT(2, 0);
#if defined(_WIN32)
void StringAppendV(std::wstring* dst,
                               const wchar_t* format,
                               va_list ap) WPRINTF_FORMAT(2, 0);
#endif

#endif // H_STRINGPRINTF
