// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_STRING_UTIL_H
#define CORE_STRING_UTIL_H

#include <ctype.h>
#include <stdarg.h>  // va_list
#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "core/compiler_specific.hpp"

#include <absl/strings/string_view.h>

// C standard-library functions that aren't cross-platform are provided as
// "...", and their prototypes are listed below. These functions are
// then implemented as inline calls to the platform-specific equivalents in the
// platform-specific headers.

namespace internal {
// Wrapper for vsnprintf that always null-terminates and always returns the
// number of characters that would be in an untruncated formatted
// string, even when truncation occurs.
int vsnprintf(char* buffer, size_t size, const char* format, va_list arguments)
    PRINTF_FORMAT(3, 0);

// Some of these implementations need to be inlined.

// We separate the declaration from the implementation of this inline
// function just so the PRINTF_FORMAT works.
inline int snprintf(char* buffer, size_t size, const char* format, ...)
    PRINTF_FORMAT(3, 4);
inline int snprintf(char* buffer, size_t size, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = vsnprintf(buffer, size, format, arguments);
  va_end(arguments);
  return result;
}

// BSD-style safe and consistent string copy functions.
// Copies |src| to |dst|, where |dst_size| is the total allocated size of |dst|.
// Copies at most |dst_size|-1 characters, and always NULL terminates |dst|, as
// long as |dst_size| is not 0.  Returns the length of |src| in characters.
// If the return value is >= dst_size, then the output was truncated.
// NOTE: All sizes are in number of characters, NOT in bytes.
size_t strlcpy(char* dst, const char* src, size_t dst_size);
size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t dst_size);
}  // namespace internal

// Scan a wprintf format string to determine whether it's portable across a
// variety of systems.  This function only checks that the conversion
// specifiers used by the format string are supported and have the same meaning
// on a variety of systems.  It doesn't check for other errors that might occur
// within a format string.
//
// Nonportable conversion specifiers for wprintf are:
//  - 's' and 'c' without an 'l' length modifier.  %s and %c operate on char
//     data on all systems except Windows, which treat them as wchar_t data.
//     Use %ls and %lc for wchar_t data instead.
//  - 'S' and 'C', which operate on wchar_t data on all systems except Windows,
//     which treat them as char data.  Use %ls and %lc for wchar_t data
//     instead.
//  - 'F', which is not identified by Windows wprintf documentation.
//  - 'D', 'O', and 'U', which are deprecated and not available on all systems.
//     Use %ld, %lo, and %lu instead.
//
// Note that there is no portable conversion specifier for char data when
// working with wprintf.
//
// This function is intended to be called from vswprintf.
bool IsWprintfFormatPortable(const wchar_t* format);

// Simplified implementation of C++20's std::basic_string_view(It, End).
// Reference: https://wg21.link/string.view.cons
template <typename Iter>
constexpr absl::string_view MakeStringView(Iter begin, Iter end) {
  DCHECK_GE(end - begin, 0);
  return {to_address(begin), static_cast<size_t>(end - begin)};
}

template <typename Iter>
constexpr std::wstring_view MakeWStringView(Iter begin, Iter end) {
  DCHECK_GE(end - begin, 0);
  return {to_address(begin), static_cast<size_t>(end - begin)};
}

// ASCII-specific tolower.  The standard library's tolower is locale sensitive,
// so we don't want to use it here.
template <typename CharT,
          typename = std::enable_if_t<std::is_integral<CharT>::value>>
CharT ToLowerASCII(CharT c) {
  return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

// ASCII-specific toupper.  The standard library's toupper is locale sensitive,
// so we don't want to use it here.
template <typename CharT,
          typename = std::enable_if_t<std::is_integral<CharT>::value>>
CharT ToUpperASCII(CharT c) {
  return (c >= 'a' && c <= 'z') ? (c + ('A' - 'a')) : c;
}

// Converts the given string to it's ASCII-lowercase equivalent.
std::string ToLowerASCII(absl::string_view str);

// Converts the given string to it's ASCII-uppercase equivalent.
std::string ToUpperASCII(absl::string_view str);

// Functor for case-insensitive ASCII comparisons for STL algorithms like
// std::search.
//
// Note that a full Unicode version of this functor is not possible to write
// because case mappings might change the number of characters, depend on
// context (combining accents), and require handling UTF-16. If you need
// proper Unicode support, use i18n::ToLower/FoldCase and then just
// use a normal operator== on the result.
template <typename Char>
struct CaseInsensitiveCompareASCII {
 public:
  bool operator()(Char x, Char y) const {
    return ToLowerASCII(x) == ToLowerASCII(y);
  }
};

// Like strcasecmp for case-insensitive ASCII characters only. Returns:
//   -1  (a < b)
//    0  (a == b)
//    1  (a > b)
// (unlike strcasecmp which can return values greater or less than 1/-1). For
// full Unicode support, use i18n::ToLower or i18h::FoldCase
// and then just call the normal string operators on the result.
int CompareCaseInsensitiveASCII(absl::string_view a, absl::string_view b);

// Equality for ASCII case-insensitive comparisons. For full Unicode support,
// use i18n::ToLower or i18h::FoldCase and then compare with either
// == or !=.
bool EqualsCaseInsensitiveASCII(absl::string_view a, absl::string_view b);

// These threadsafe functions return references to globally unique empty
// strings.
//
// It is likely faster to construct a new empty string object (just a few
// instructions to set the length to 0) than to get the empty string instance
// returned by these functions (which requires threadsafe static access).
//
// Therefore, DO NOT USE THESE AS A GENERAL-PURPOSE SUBSTITUTE FOR DEFAULT
// CONSTRUCTORS. There is only one case where you should use these: functions
// which need to return a string by reference (e.g. as a class member
// accessor), and don't have an empty string to use (e.g. in an error case).
// These should not be used as initializers, function arguments, or return
// values for functions which return by value or outparam.
const std::string& EmptyString();
const std::u16string& EmptyString16();

// Contains the set of characters representing whitespace in the corresponding
// encoding. Null-terminated. The ASCII versions are the whitespaces as defined
// by HTML5, and don't include control characters.
extern const wchar_t kWhitespaceWide[];          // Includes Unicode.
extern const char16_t kWhitespaceUTF16[];        // Includes Unicode.
extern const char16_t kWhitespaceNoCrLfUTF16[];  // Unicode w/o CR/LF.
extern const char kWhitespaceASCII[];
extern const char16_t kWhitespaceASCIIAs16[];  // No unicode.

// Null-terminated string representing the UTF-8 byte order mark.
extern const char kUtf8ByteOrderMark[];

// Removes characters in |remove_chars| from anywhere in |input|.  Returns true
// if any characters were removed.  |remove_chars| must be null-terminated.
// NOTE: Safe to use the same variable for both |input| and |output|.
bool RemoveChars(absl::string_view input,
                 absl::string_view remove_chars,
                 std::string* output);

// Replaces characters in |replace_chars| from anywhere in |input| with
// |replace_with|.  Each character in |replace_chars| will be replaced with
// the |replace_with| string.  Returns true if any characters were replaced.
// |replace_chars| must be null-terminated.
// NOTE: Safe to use the same variable for both |input| and |output|.
bool ReplaceChars(absl::string_view input,
                  absl::string_view replace_chars,
                  absl::string_view replace_with,
                  std::string* output);

enum TrimPositions {
  TRIM_NONE = 0,
  TRIM_LEADING = 1 << 0,
  TRIM_TRAILING = 1 << 1,
  TRIM_ALL = TRIM_LEADING | TRIM_TRAILING,
};

// Removes characters in |trim_chars| from the beginning and end of |input|.
// The 8-bit version only works on 8-bit characters, not UTF-8. Returns true if
// any characters were removed.
//
// It is safe to use the same variable for both |input| and |output| (this is
// the normal usage to trim in-place).
bool TrimString(absl::string_view input,
                absl::string_view trim_chars,
                std::string* output);

// absl::string_view versions of the above. The returned pieces refer to the
// original buffer.
absl::string_view TrimString(absl::string_view input,
                             absl::string_view trim_chars,
                             TrimPositions positions);

// Trims any whitespace from either end of the input string.
//
// The absl::string_view versions return a substring referencing the input
// buffer. The ASCII versions look only for ASCII whitespace.
//
// The std::string versions return where whitespace was found.
// NOTE: Safe to use the same variable for both input and output.
TrimPositions TrimWhitespaceASCII(absl::string_view input,
                                  TrimPositions positions,
                                  std::string* output);
absl::string_view TrimWhitespaceASCII(absl::string_view input,
                                      TrimPositions positions);

// Searches for CR or LF characters.  Removes all contiguous whitespace
// strings that contain them.  This is useful when trying to deal with text
// copied from terminals.
// Returns |text|, with the following three transformations:
// (1) Leading and trailing whitespace is trimmed.
// (2) If |trim_sequences_with_line_breaks| is true, any other whitespace
//     sequences containing a CR or LF are trimmed.
// (3) All other whitespace sequences are converted to single spaces.
std::string CollapseWhitespaceASCII(absl::string_view text,
                                    bool trim_sequences_with_line_breaks);

// Returns true if |input| is empty or contains only characters found in
// |characters|.
bool ContainsOnlyChars(absl::string_view input, absl::string_view characters);

// Returns true if |str| is structurally valid UTF-8 and also doesn't
// contain any non-character code point (e.g. U+10FFFE). Prohibiting
// non-characters increases the likelihood of detecting non-UTF-8 in
// real-world text, for callers which do not need to accept
// non-characters in strings.
bool IsStringUTF8(absl::string_view str);

// Returns true if |str| contains valid UTF-8, allowing non-character
// code points.
bool IsStringUTF8AllowingNoncharacters(absl::string_view str);

// Returns true if |str| contains only valid ASCII character values.
// Note 1: IsStringASCII executes in time determined solely by the
// length of the string, not by its contents, so it is robust against
// timing attacks for all strings of equal length.
// Note 2: IsStringASCII assumes the input is likely all ASCII, and
// does not leave early if it is not the case.
bool IsStringASCII(absl::string_view str);

#if defined(WCHAR_T_IS_32_BIT)
bool IsStringASCII(std::wstring_view str);
#endif

// Compare the lower-case form of the given string against the given
// previously-lower-cased ASCII string (typically a constant).
bool LowerCaseEqualsASCII(absl::string_view str,
                          absl::string_view lowercase_ascii);
bool LowerCaseEqualsASCII(const std::u16string& str,
                          absl::string_view lowercase_ascii);

// Indicates case sensitivity of comparisons. Only ASCII case insensitivity
// is supported. Full Unicode case-insensitive conversions would need to go in
// base/i18n so it can use ICU.
//
// If you need to do Unicode-aware case-insensitive StartsWith/EndsWith, it's
// best to call i18n::ToLower() or i18n::FoldCase() (see
// base/i18n/case_conversion.h for usage advice) on the arguments, and then use
// the results to a case-sensitive comparison.
enum class CompareCase {
  SENSITIVE,
  INSENSITIVE_ASCII,
};

bool StartsWith(absl::string_view str,
                absl::string_view search_for,
                CompareCase case_sensitivity = CompareCase::SENSITIVE);
bool EndsWith(absl::string_view str,
              absl::string_view search_for,
              CompareCase case_sensitivity = CompareCase::SENSITIVE);

// Determines the type of ASCII character, independent of locale (the C
// library versions will change based on locale).
template <typename Char>
inline bool IsAsciiWhitespace(Char c) {
  return c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '\f';
}
template <typename Char>
inline bool IsAsciiAlpha(Char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
template <typename Char>
inline bool IsAsciiUpper(Char c) {
  return c >= 'A' && c <= 'Z';
}
template <typename Char>
inline bool IsAsciiLower(Char c) {
  return c >= 'a' && c <= 'z';
}
template <typename Char>
inline bool IsAsciiDigit(Char c) {
  return c >= '0' && c <= '9';
}
template <typename Char>
inline bool IsAsciiPrintable(Char c) {
  return c >= ' ' && c <= '~';
}

template <typename Char>
inline bool IsHexDigit(Char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

// Returns the integer corresponding to the given hex character. For example:
//    '4' -> 4
//    'a' -> 10
//    'B' -> 11
// Assumes the input is a valid hex character. DCHECKs in debug builds if not.
char HexDigitToInt(wchar_t c);

// Returns true if it's a Unicode whitespace character.
bool IsUnicodeWhitespace(wchar_t c);

// Starting at |start_offset| (usually 0), replace the first instance of
// |find_this| with |replace_with|.
void ReplaceFirstSubstringAfterOffset(std::string* str,
                                      size_t start_offset,
                                      absl::string_view find_this,
                                      absl::string_view replace_with);

// Starting at |start_offset| (usually 0), look through |str| and replace all
// instances of |find_this| with |replace_with|.
//
// This does entire substrings; use std::replace in <algorithm> for single
// characters, for example:
//   std::replace(str.begin(), str.end(), 'a', 'b');
void ReplaceSubstringsAfterOffset(std::string* str,
                                  size_t start_offset,
                                  absl::string_view find_this,
                                  absl::string_view replace_with);

// Reserves enough memory in |str| to accommodate |length_with_null| characters,
// sets the size of |str| to |length_with_null - 1| characters, and returns a
// pointer to the underlying contiguous array of characters.  This is typically
// used when calling a function that writes results into a character array, but
// the caller wants the data to be managed by a string-like object.  It is
// convenient in that is can be used inline in the call, and fast in that it
// avoids copying the results of the call from a char* into a string.
//
// Internally, this takes linear time because the resize() call 0-fills the
// underlying array for potentially all
// (|length_with_null - 1| * sizeof(string_type::value_type)) bytes.  Ideally we
// could avoid this aspect of the resize() call, as we expect the caller to
// immediately write over this memory, but there is no other way to set the size
// of the string, and not doing that will mean people who access |str| rather
// than str.c_str() will get back a string of whatever size |str| had on entry
// to this function (probably 0).
char* WriteInto(std::string* str, size_t length_with_null);
char16_t* WriteInto(std::u16string* str, size_t length_with_null);

// Explicit initializer_list overloads are required to break ambiguity when used
// with a literal initializer list (otherwise the compiler would not be able to
// decide between the string and absl::string_view overloads).
std::string JoinString(std::initializer_list<absl::string_view> parts,
                       absl::string_view separator);

#endif  // CORE_STRING_UTIL_H
