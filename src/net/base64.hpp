// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#ifndef NET_BASE64_H_
#define NET_BASE64_H_

#include <stdint.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "core/span.hpp"

namespace net {

// Encodes the input binary data in base64.
std::string Base64Encode(span<const uint8_t> input);

// Encodes the input binary data in base64 and appends it to the output.
void Base64EncodeAppend(span<const uint8_t> input, std::string* output);

// Decodes the base64 input string.  Returns true if successful and false
// otherwise. The output string is only modified if successful. The decoding can
// be done in-place.
enum class Base64DecodePolicy {
  // Input should match the output format of Base64Encode. i.e.
  // - Input length should be divisible by 4
  // - Maximum of 2 padding characters
  // - No non-base64 characters.
  kStrict,

  // Matches https://infra.spec.whatwg.org/#forgiving-base64-decode.
  // - Removes all ascii whitespace
  // - Maximum of 2 padding characters
  // - Allows input length not divisible by 4 if no padding chars are added.
  kForgiving,
};
bool Base64Decode(std::string_view input, std::string* output, Base64DecodePolicy policy = Base64DecodePolicy::kStrict);

// Decodes the base64 input string. Returns `std::nullopt` if unsuccessful.
std::optional<std::vector<uint8_t>> Base64Decode(std::string_view input);

}  // namespace net

#endif  // NET_BASE64_H_
