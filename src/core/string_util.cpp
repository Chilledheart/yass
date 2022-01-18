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

std::string ToLowerASCII(absl::string_view str) {
  return internal::ToLowerASCIIImpl(str);
}

std::string ToUpperASCII(absl::string_view str) {
  return internal::ToUpperASCIIImpl(str);
}

int CompareCaseInsensitiveASCII(absl::string_view a, absl::string_view b) {
  return internal::CompareCaseInsensitiveASCIIT(a, b);
}

bool EqualsCaseInsensitiveASCII(absl::string_view a, absl::string_view b) {
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

bool ReplaceChars(absl::string_view input,
                  absl::string_view replace_chars,
                  absl::string_view replace_with,
                  std::string* output) {
  return internal::ReplaceCharsT(input, replace_chars, replace_with, output);
}

bool RemoveChars(absl::string_view input,
                 absl::string_view remove_chars,
                 std::string* output) {
  return internal::ReplaceCharsT(input, remove_chars, absl::string_view(),
                                 output);
}

bool TrimString(absl::string_view input,
                absl::string_view trim_chars,
                std::string* output) {
  return internal::TrimStringT(input, trim_chars, TRIM_ALL, output) !=
         TRIM_NONE;
}

absl::string_view TrimString(absl::string_view input,
                             absl::string_view trim_chars,
                             TrimPositions positions) {
  return internal::TrimStringViewT(input, trim_chars, positions);
}

TrimPositions TrimWhitespaceASCII(absl::string_view input,
                                  TrimPositions positions,
                                  std::string* output) {
  return internal::TrimStringT(input, absl::string_view(kWhitespaceASCII),
                               positions, output);
}

absl::string_view TrimWhitespaceASCII(absl::string_view input,
                                      TrimPositions positions) {
  return internal::TrimStringViewT(input, absl::string_view(kWhitespaceASCII),
                                   positions);
}

std::string CollapseWhitespaceASCII(absl::string_view text,
                                    bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

bool ContainsOnlyChars(absl::string_view input, absl::string_view characters) {
  return input.find_first_not_of(characters) == absl::string_view::npos;
}

bool LowerCaseEqualsASCII(absl::string_view str,
                          absl::string_view lowercase_ascii) {
  return internal::DoLowerCaseEqualsASCII(str, lowercase_ascii);
}

bool StartsWith(absl::string_view str,
                absl::string_view search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

bool EndsWith(absl::string_view str,
              absl::string_view search_for,
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
                                      absl::string_view find_this,
                                      absl::string_view replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_FIRST);
}

void ReplaceSubstringsAfterOffset(std::string* str,
                                  size_t start_offset,
                                  absl::string_view find_this,
                                  absl::string_view replace_with) {
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

std::string JoinString(std::initializer_list<absl::string_view> parts,
                       absl::string_view separator) {
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
