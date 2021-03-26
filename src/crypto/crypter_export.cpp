// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "crypto/crypter_export.hpp"

enum cipher_method cipher_method = CRYPTO_INVALID;

#define XX(num, name, string) const char *CRYPTO_##name##_STR = string;
CIPHER_METHOD_MAP(XX)
#undef XX

enum cipher_method to_cipher_method(const std::string &method) {
#define XX(num, name, string)                                                  \
  if (method == string) {                                                      \
    return CRYPTO_##name;                                                      \
  }
  CIPHER_METHOD_MAP(XX)
#undef XX
  return CRYPTO_INVALID;
}

const char *to_cipher_method_str(enum cipher_method method) {
#define XX(num, name, string)                                                  \
  if (method == num) {                                                         \
    return string;                                                             \
  }
  CIPHER_METHOD_MAP(XX)
#undef XX
  return CRYPTO_INVALID_STR;
}
