//
// crypter_export.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "crypto/crypter_export.hpp"

enum cipher_method cipher_method = CRYPTO_PLAINTEXT;

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
  return CRYPTO_PLAINTEXT;
}

const char *to_cipher_method_str(enum cipher_method method) {
#define XX(num, name, string)                                                  \
  if (method == num) {                                                         \
    return string;                                                             \
  }
  CIPHER_METHOD_MAP(XX)
#undef XX
  return CRYPTO_PLAINTEXT_STR;
}
