// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/escape.h"

#include <ostream>

#include "core/check_op.hpp"
#include "core/cxx17_backports.hpp"
#include "core/string_util.hpp"
#include "core/utf_string_conversion_utils.hpp"
#include "icu/icu_utf.h"

namespace net {

namespace {

const char kHexString[] = "0123456789ABCDEF";
inline char IntToHex(int i) {
  DCHECK_GE(i, 0) << i << " not a hex value";
  DCHECK_LE(i, 15) << i << " not a hex value";
  return kHexString[i];
}

// A fast bit-vector map for ascii characters.
//
// Internally stores 256 bits in an array of 8 ints.
// Does quick bit-flicking to lookup needed characters.
struct Charmap {
  bool Contains(unsigned char c) const {
    return ((map[c >> 5] & (1 << (c & 31))) != 0);
  }

  uint32_t map[8];
};

// Given text to escape and a Charmap defining which values to escape,
// return an escaped string.  If use_plus is true, spaces are converted
// to +, otherwise, if spaces are in the charmap, they are converted to
// %20. And if keep_escaped is true, %XX will be kept as it is, otherwise, if
// '%' is in the charmap, it is converted to %25.
std::string Escape(absl::string_view text,
                   const Charmap& charmap,
                   bool use_plus,
                   bool keep_escaped = false) {
  std::string escaped;
  escaped.reserve(text.length() * 3);
  for (unsigned int i = 0; i < text.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (use_plus && ' ' == c) {
      escaped.push_back('+');
    } else if (keep_escaped && '%' == c && i + 2 < text.length() &&
               IsHexDigit(text[i + 1]) && IsHexDigit(text[i + 2])) {
      escaped.push_back('%');
    } else if (charmap.Contains(c)) {
      escaped.push_back('%');
      escaped.push_back(IntToHex(c >> 4));
      escaped.push_back(IntToHex(c & 0xf));
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

// Convert a character |c| to a form that will not be mistaken as HTML.
template <class str>
void AppendEscapedCharForHTMLImpl(typename str::value_type c, str* output) {
  static constexpr struct {
    char key;
    absl::string_view replacement;
  } kCharsToEscape[] = {
      {'<', "&lt;"},   {'>', "&gt;"},   {'&', "&amp;"},
      {'"', "&quot;"}, {'\'', "&#39;"},
  };
  for (const auto& char_to_escape : kCharsToEscape) {
    if (c == char_to_escape.key) {
      output->append(std::begin(char_to_escape.replacement),
                     std::end(char_to_escape.replacement));
      return;
    }
  }
  output->push_back(c);
}

// Convert |input| string to a form that will not be interpreted as HTML.
template <typename T, typename CharT = typename T::value_type>
std::basic_string<CharT> EscapeForHTMLImpl(T input) {
  std::basic_string<CharT> result;
  result.reserve(input.size());  // Optimize for no escaping.

  for (auto c : input) {
    AppendEscapedCharForHTMLImpl(c, &result);
  }

  return result;
}

// Everything except alphanumerics and -._~
// See RFC 3986 for the list of unreserved characters.
static const Charmap kUnreservedCharmap = {
    {0xffffffffL, 0xfc009fffL, 0x78000001L, 0xb8000001L, 0xffffffffL,
     0xffffffffL, 0xffffffffL, 0xffffffffL}};

// Everything except alphanumerics and !'()*-._~
// See RFC 2396 for the list of reserved characters.
static const Charmap kQueryCharmap = {{
  0xffffffffL, 0xfc00987dL, 0x78000001L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL
}};

// non-printable, non-7bit, and (including space)  "#%:<>?[\]^`{|}
static const Charmap kPathCharmap = {{
  0xffffffffL, 0xd400002dL, 0x78000000L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL
}};

#ifdef OS_APPLE
// non-printable, non-7bit, and (including space)  "#%<>[\]^`{|}
static const Charmap kNSURLCharmap = {{
  0xffffffffL, 0x5000002dL, 0x78000000L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL
}};
#endif  // OS_APPLE

// non-printable, non-7bit, and (including space) ?>=<;+'&%$#"![\]^`{|}
static const Charmap kUrlEscape = {{
  0xffffffffL, 0xf80008fdL, 0x78000001L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL
}};

// non-7bit, as well as %.
static const Charmap kNonASCIICharmapAndPercent = {
    {0x00000000L, 0x00000020L, 0x00000000L, 0x00000000L, 0xffffffffL,
     0xffffffffL, 0xffffffffL, 0xffffffffL}};

// non-7bit
static const Charmap kNonASCIICharmap = {{0x00000000L, 0x00000000L, 0x00000000L,
                                          0x00000000L, 0xffffffffL, 0xffffffffL,
                                          0xffffffffL, 0xffffffffL}};

// Everything except alphanumerics, the reserved characters(;/?:@&=+$,) and
// !'()*-._~#[]
static const Charmap kExternalHandlerCharmap = {{
  0xffffffffL, 0x50000025L, 0x50000000L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL
}};

}  // namespace

std::string EscapeAllExceptUnreserved(absl::string_view text) {
  return Escape(text, kUnreservedCharmap, false);
}

std::string EscapeQueryParamValue(absl::string_view text, bool use_plus) {
  return Escape(text, kQueryCharmap, use_plus);
}

std::string EscapePath(absl::string_view path) {
  return Escape(path, kPathCharmap, false);
}

#ifdef OS_APPLE
std::string EscapeNSURLPrecursor(absl::string_view precursor) {
  return Escape(precursor, kNSURLCharmap, false, true);
}
#endif  // OS_APPLE

std::string EscapeUrlEncodedData(absl::string_view path, bool use_plus) {
  return Escape(path, kUrlEscape, use_plus);
}

std::string EscapeNonASCIIAndPercent(absl::string_view input) {
  return Escape(input, kNonASCIICharmapAndPercent, false);
}

std::string EscapeNonASCII(absl::string_view input) {
  return Escape(input, kNonASCIICharmap, false);
}

std::string EscapeExternalHandlerValue(absl::string_view text) {
  return Escape(text, kExternalHandlerCharmap, false, true);
}

void AppendEscapedCharForHTML(char c, std::string* output) {
  AppendEscapedCharForHTMLImpl(c, output);
}

std::string EscapeForHTML(absl::string_view input) {
  return EscapeForHTMLImpl(input);
}

// TODO(crbug/1100760): Move functions from net/base/escape to
// base/strings/escape.
std::string UnescapeURLComponent(absl::string_view escaped_text,
                                 UnescapeRule::Type rules) {
  return ::UnescapeURLComponent(escaped_text, rules);
}

std::string UnescapeBinaryURLComponent(absl::string_view escaped_text,
                                       UnescapeRule::Type rules) {
  return ::UnescapeBinaryURLComponent(escaped_text, rules);
}

bool UnescapeBinaryURLComponentSafe(absl::string_view escaped_text,
                                    bool fail_on_path_separators,
                                    std::string* unescaped_text) {
  return ::UnescapeBinaryURLComponentSafe(
      escaped_text, fail_on_path_separators, unescaped_text);
}

bool ContainsEncodedBytes(absl::string_view escaped_text,
                          const std::set<unsigned char>& bytes) {
  return ::ContainsEncodedBytes(escaped_text, bytes);
}

}  // namespace net
