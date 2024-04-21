// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#include "crypto/crypter_export.hpp"

enum cipher_method to_cipher_method(const std::string_view& method) {
#define XX(num, name, string) \
  if (method == string) {     \
    return CRYPTO_##name;     \
  }
  CIPHER_METHOD_MAP(XX)
#undef XX
  return CRYPTO_INVALID;
}

std::string_view to_cipher_method_name(enum cipher_method method) {
#define XX(num, name, string)                \
  if (method == num) {                       \
    constexpr std::string_view _ret = #name; \
    return _ret;                             \
  }
  CIPHER_METHOD_MAP(XX)
#undef XX
  return CRYPTO_INVALID_STR;
}

std::string_view to_cipher_method_str(enum cipher_method method) {
#define XX(num, name, string)                 \
  if (method == num) {                        \
    constexpr std::string_view _ret = string; \
    return _ret;                              \
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
static constexpr const char kCipherMethodsStrImpl[] = CIPHER_METHOD_VALID_MAP(XX)
#undef XX
    ;
constexpr const std::string_view kCipherMethodsStr(kCipherMethodsStrImpl, std::size(kCipherMethodsStrImpl) - 3);
