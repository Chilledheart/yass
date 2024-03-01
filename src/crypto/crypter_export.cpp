// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "crypto/crypter_export.hpp"

#define XX(num, name, string) const char* CRYPTO_##name##_STR = string;
CIPHER_METHOD_MAP(XX)
#undef XX

enum cipher_method to_cipher_method(const std::string& method) {
#define XX(num, name, string) \
  if (method == string) {     \
    return CRYPTO_##name;     \
  }
  CIPHER_METHOD_MAP(XX)
#undef XX
  return CRYPTO_INVALID;
}

const char* to_cipher_method_name(enum cipher_method method) {
#define XX(num, name, string) \
  if (method == num) {        \
    return #name;             \
  }
  CIPHER_METHOD_MAP(XX)
#undef XX
  return CRYPTO_INVALID_STR;
}

const char* to_cipher_method_str(enum cipher_method method) {
#define XX(num, name, string) \
  if (method == num) {        \
    return string;            \
  }
  CIPHER_METHOD_MAP(XX)
#undef XX
  return CRYPTO_INVALID_STR;
}

bool is_valid_cipher_method(enum cipher_method method) {
#define XX(num, name, string) \
  if (method == num) {        \
    return true;              \
  }
  CIPHER_METHOD_MAP(XX)
#undef XX
  return false;
}

#define XX(num, name, string) string ", "
constexpr const char kCipherMethodsStr[] = CIPHER_METHOD_VALID_MAP(XX)
#undef XX
    ;
