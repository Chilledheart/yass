// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/base64.hpp"

#include <stddef.h>

#include <openssl/base64.h>

std::string Base64Encode(const uint8_t* data, size_t length) {
  std::string output;
  size_t out_len;
  EVP_EncodedLength(&out_len, length);  // makes room for null byte
  output.resize(out_len);

  // modp_b64_encode_len() returns at least 1, so output[0] is safe to use.
  const size_t output_size =
      EVP_EncodeBlock(reinterpret_cast<uint8_t*>(&(output[0])), data, length);

  output.resize(output_size);
  return output;
}

void Base64Encode(absl::string_view input, std::string* output) {
  *output = Base64Encode(reinterpret_cast<const uint8_t*>(&*input.begin()),
                         input.size());
}

bool Base64Decode(absl::string_view input, std::string* output) {
  std::string temp;
  size_t out_len;
  EVP_DecodedLength(&out_len, input.size());
  temp.resize(out_len);

  // does not null terminate result since result is binary data!
  size_t input_size = input.size();
  int output_size = EVP_DecodeBase64(
      reinterpret_cast<uint8_t*>(&(temp[0])), &out_len, out_len,
      reinterpret_cast<const uint8_t*>(input.data()), input_size);
  if (output_size <= 0)
    return false;

  temp.resize(output_size);
  output->swap(temp);
  return true;
}

absl::optional<std::vector<uint8_t>> Base64Decode(absl::string_view input) {
  std::vector<uint8_t> ret;
  size_t out_len;
  EVP_DecodedLength(&out_len, input.size());
  ret.resize(out_len);

  size_t input_size = input.size();
  int output_size = EVP_DecodeBase64(
      ret.data(), &out_len, out_len,
      reinterpret_cast<const uint8_t*>(input.data()), input_size);
  if (output_size <= 0)
    return absl::nullopt;

  ret.resize(output_size);
  return ret;
}
