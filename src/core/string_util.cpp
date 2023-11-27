// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/string_util.hpp"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <vector>

#include "core/string_util_internal.hpp"
#include "core/template_util.hpp"
#include "core/utf_string_conversion_utils.hpp"
#include "icu/icu_utf.h"

bool IsWprintfFormatPortable(const wchar_t* format) {
  for (const wchar_t* position = format; *position != '\0'; ++position) {
    if (*position == '%') {
      bool in_specification = true;
      bool modifier_l = false;
      while (in_specification) {
        // Eat up characters until reaching a known specifier.
        if (*++position == '\0') {
          // The format string ended in the middle of a specification.  Call
          // it portable because no unportable specifications were found.  The
          // string is equally broken on all platforms.
          return true;
        }

        if (*position == 'l') {
          // 'l' is the only thing that can save the 's' and 'c' specifiers.
          modifier_l = true;
        } else if (((*position == 's' || *position == 'c') && !modifier_l) ||
                   *position == 'S' || *position == 'C' || *position == 'F' ||
                   *position == 'D' || *position == 'O' || *position == 'U') {
          // Not portable.
          return false;
        }

        if (wcschr(L"diouxXeEfgGaAcspn%", *position)) {
          // Portable, keep scanning the rest of the format string.
          in_specification = false;
        }
      }
    }
  }

  return true;
}

std::string ToLowerASCII(std::string_view str) {
  return internal::ToLowerASCIIImpl(str);
}

std::string ToUpperASCII(std::string_view str) {
  return internal::ToUpperASCIIImpl(str);
}

int CompareCaseInsensitiveASCII(std::string_view a, std::string_view b) {
  return internal::CompareCaseInsensitiveASCIIT(a, b);
}

bool EqualsCaseInsensitiveASCII(std::string_view a, std::string_view b) {
  return a.size() == b.size() &&
         internal::CompareCaseInsensitiveASCIIT(a, b) == 0;
}

const std::string& EmptyString() {
  static const NoDestructor<std::string> s;
  return *s;
}

const std::u16string& EmptyString16() {
  static const NoDestructor<std::u16string> s16;
  return *s16;
}

bool ReplaceChars(std::string_view input,
                  std::string_view replace_chars,
                  std::string_view replace_with,
                  std::string* output) {
  return internal::ReplaceCharsT(input, replace_chars, replace_with, output);
}

bool RemoveChars(std::string_view input,
                 std::string_view remove_chars,
                 std::string* output) {
  return internal::ReplaceCharsT(input, remove_chars, std::string_view(),
                                 output);
}

bool TrimString(std::string_view input,
                std::string_view trim_chars,
                std::string* output) {
  return internal::TrimStringT(input, trim_chars, TRIM_ALL, output) !=
         TRIM_NONE;
}

std::string_view TrimString(std::string_view input,
                             std::string_view trim_chars,
                             TrimPositions positions) {
  return internal::TrimStringViewT(input, trim_chars, positions);
}

TrimPositions TrimWhitespaceASCII(std::string_view input,
                                  TrimPositions positions,
                                  std::string* output) {
  return internal::TrimStringT(input, std::string_view(kWhitespaceASCII),
                               positions, output);
}

std::string_view TrimWhitespaceASCII(std::string_view input,
                                      TrimPositions positions) {
  return internal::TrimStringViewT(input, std::string_view(kWhitespaceASCII),
                                   positions);
}

std::string CollapseWhitespaceASCII(std::string_view text,
                                    bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

bool ContainsOnlyChars(std::string_view input, std::string_view characters) {
  return input.find_first_not_of(characters) == std::string_view::npos;
}

bool IsStringASCII(std::string_view str) {
  return internal::DoIsStringASCII(str.data(), str.length());
}

#if defined(WCHAR_T_IS_32_BIT)
bool IsStringASCII(std::wstring_view str) {
  return internal::DoIsStringASCII(str.data(), str.length());
}
#endif

bool IsStringUTF8(std::string_view str) {
  return internal::DoIsStringUTF8<IsValidCharacter>(str);
}

bool IsStringUTF8AllowingNoncharacters(std::string_view str) {
  return internal::DoIsStringUTF8<IsValidCodepoint>(str);
}

bool LowerCaseEqualsASCII(std::string_view str,
                          std::string_view lowercase_ascii) {
  return internal::DoLowerCaseEqualsASCII(str, lowercase_ascii);
}

bool LowerCaseEqualsASCII(const std::u16string& str,
                          std::string_view lowercase_ascii) {
  return internal::DoLowerCaseEqualsASCII(str, lowercase_ascii);
}

bool StartsWith(std::string_view str,
                std::string_view search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

bool EndsWith(std::string_view str,
              std::string_view search_for,
              CompareCase case_sensitivity) {
  return internal::EndsWithT(str, search_for, case_sensitivity);
}

char HexDigitToInt(wchar_t c) {
  DCHECK(IsHexDigit(c));
  if (c >= '0' && c <= '9')
    return static_cast<char>(c - '0');
  if (c >= 'A' && c <= 'F')
    return static_cast<char>(c - 'A' + 10);
  if (c >= 'a' && c <= 'f')
    return static_cast<char>(c - 'a' + 10);
  return 0;
}

bool IsUnicodeWhitespace(wchar_t c) {
  // kWhitespaceWide is a NULL-terminated string
  for (const wchar_t* cur = kWhitespaceWide; *cur; ++cur) {
    if (*cur == c)
      return true;
  }
  return false;
}

void ReplaceFirstSubstringAfterOffset(std::string* str,
                                      size_t start_offset,
                                      std::string_view find_this,
                                      std::string_view replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_FIRST);
}

void ReplaceSubstringsAfterOffset(std::string* str,
                                  size_t start_offset,
                                  std::string_view find_this,
                                  std::string_view replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_ALL);
}

char* WriteInto(std::string* str, size_t length_with_null) {
  return internal::WriteIntoT(str, length_with_null);
}

char16_t* WriteInto(std::u16string* str, size_t length_with_null) {
  return internal::WriteIntoT(str, length_with_null);
}

std::string JoinString(std::initializer_list<std::string_view> parts,
                       std::string_view separator) {
  return internal::JoinStringT(parts, separator);
}

namespace internal {

size_t strlcpy(char* dst, const char* src, size_t dst_size) {
  return ::internal::lcpyT(dst, src, dst_size);
}
size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t dst_size) {
  return ::internal::lcpyT(dst, src, dst_size);
}

}  // namespace internal
