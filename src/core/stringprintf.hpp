// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#ifndef H_STRINGPRINTF
#define H_STRINGPRINTF

#include <string>

#include "core/compiler_specific.hpp"

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
const std::string& SStringPrintf(std::string* dst, const char* format, ...)
    PRINTF_FORMAT(2, 3);
#if defined(_WIN32)
const std::wstring& SStringPrintf(std::wstring* dst, const wchar_t* format, ...)
    WPRINTF_FORMAT(2, 3);
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
void StringAppendV(std::wstring* dst, const wchar_t* format, va_list ap)
    WPRINTF_FORMAT(2, 0);
#endif

#endif  // H_STRINGPRINTF
