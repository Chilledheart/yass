// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_BASE64_H_
#define CORE_BASE64_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include <absl/types/optional.h>

// Encodes the input binary data in base64.
std::string Base64Encode(const uint8_t* data, size_t length);

// Encodes the input string in base64.
void Base64Encode(std::string_view input, std::string* output);

// Decodes the base64 input string.  Returns true if successful and false
// otherwise. The output string is only modified if successful. The decoding can
// be done in-place.
bool Base64Decode(std::string_view input, std::string* output);

// Decodes the base64 input string. Returns `std::nullopt` if unsuccessful.
std::optional<std::vector<uint8_t>> Base64Decode(std::string_view input);

#endif  // CORE_BASE64_H_
