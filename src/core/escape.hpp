// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_STRINGS_ESCAPE_H_
#define CORE_STRINGS_ESCAPE_H_

#include <stdint.h>

#include <set>
#include <string>

#include "core/utf_offset_string_conversions.hpp"

#include <absl/strings/string_view.h>

class UnescapeRule {
 public:
  // A combination of the following flags that is passed to the unescaping
  // functions.
  typedef uint32_t Type;

  enum {
    // Don't unescape anything at all.
    NONE = 0,

    // Don't unescape anything special, but all normal unescaping will happen.
    // This is a placeholder and can't be combined with other flags (since it's
    // just the absence of them). All other unescape rules imply "normal" in
    // addition to their special meaning. Things like escaped letters, digits,
    // and most symbols will get unescaped with this mode.
    NORMAL = 1 << 0,

    // Convert %20 to spaces. In some places where we're showing URLs, we may
    // want this. In places where the URL may be copied and pasted out, then
    // you wouldn't want this since it might not be interpreted in one piece
    // by other applications.  Other UTF-8 spaces will not be unescaped.
    SPACES = 1 << 1,

    // Unescapes '/' and '\\'. If these characters were unescaped, the resulting
    // URL won't be the same as the source one. Moreover, they are dangerous to
    // unescape in strings that will be used as file paths or names. This value
    // should only be used when slashes don't have special meaning, like data
    // URLs.
    PATH_SEPARATORS = 1 << 2,

    // Unescapes various characters that will change the meaning of URLs,
    // including '%', '+', '&', '#'. Does not unescape path separators.
    // If these characters were unescaped, the resulting URL won't be the same
    // as the source one. This flag is used when generating final output like
    // filenames for URLs where we won't be interpreting as a URL and want to do
    // as much unescaping as possible.
    URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS = 1 << 3,

    // URL queries use "+" for space. This flag controls that replacement.
    REPLACE_PLUS_WITH_SPACE = 1 << 4,
  };
};

// Unescapes |escaped_text| and returns the result.
// Unescaping consists of looking for the exact pattern "%XX", where each X is
// a hex digit, and converting to the character with the numerical value of
// those digits. Thus "i%20=%203%3b" unescapes to "i = 3;", if the
// "UnescapeRule::SPACES" used.
//
// This method does not ensure that the output is a valid string using any
// character encoding. However, it does leave escaped certain byte sequences
// that would be dangerous to display to the user, because if interpreted as
// UTF-8, they could be used to mislead the user. Callers that want to
// unconditionally unescape everything for uses other than displaying data to
// the user should use UnescapeBinaryURLComponent().
std::string UnescapeURLComponent(absl::string_view escaped_text,
                                 UnescapeRule::Type rules);

// Unescapes a component of a URL for use as binary data. Unlike
// UnescapeURLComponent, leaves nothing unescaped, including nulls, invalid
// characters, characters that are unsafe to display, etc. This should *not*
// be used when displaying the decoded data to the user.
//
// Only the NORMAL and REPLACE_PLUS_WITH_SPACE rules are allowed.
std::string UnescapeBinaryURLComponent(
    absl::string_view escaped_text,
    UnescapeRule::Type rules = UnescapeRule::NORMAL);

// Variant of UnescapeBinaryURLComponent().  Writes output to |unescaped_text|.
// Returns true on success, returns false and clears |unescaped_text| on
// failure. Fails on characters escaped that are unsafe to unescape in some
// contexts, which are defined as characters "\0" through "\x1F" (Which includes
// CRLF but not space), and optionally path separators. Path separators include
// both forward and backward slashes on all platforms. Does not fail if any of
// those characters appear unescaped in the input string.
bool UnescapeBinaryURLComponentSafe(absl::string_view escaped_text,
                                    bool fail_on_path_separators,
                                    std::string* unescaped_text);

// Returns true if |escaped_text| contains any element of |bytes| in
// percent-encoded form.
//
// For example, if |bytes| is {'%', '/'}, returns true if |escaped_text|
// contains "%25" or "%2F", but not if it just contains bare '%' or '/'
// characters.
bool ContainsEncodedBytes(absl::string_view escaped_text,
                          const std::set<unsigned char>& bytes);

#endif  // CORE_STRINGS_ESCAPE_H_
