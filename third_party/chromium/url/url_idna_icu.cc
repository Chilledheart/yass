// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ICU-based IDNA converter.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ostream>

#include "core/check_op.hpp"
#include "url/url_canon_internal.h"  // for _itoa_s

namespace url {


// Converts the Unicode input representing a hostname to ASCII using IDN rules.
// The output must be ASCII, but is represented as wide characters.
//
// On success, the output will be filled with the ASCII host name and it will
// return true. Unlike most other canonicalization functions, this assumes that
// the output is empty. The beginning of the host will be at offset 0, and
// the length of the output will be set to the length of the new host name.
//
// On error, this will return false. The output in this case is undefined.
// TODO(jungshik): use UTF-8/ASCII version of nameToASCII.
// Change the function signature and callers accordingly to avoid unnecessary
// conversions in our code. In addition, consider using icu::IDNA's UTF-8/ASCII
// version with StringByteSink. That way, we can avoid C wrappers and additional
// string conversion.
bool IDNToASCII(const char16_t* src, int src_len, CanonOutputW* output) {
  return false; // Not implemented
}

}  // namespace url
