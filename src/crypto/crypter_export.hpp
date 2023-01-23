// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_CRYPTO_CRYPTER_EXPORT
#define H_CRYPTO_CRYPTER_EXPORT

#include <stddef.h>
#include <stdint.h>
#include <string>

#define MAX_KEY_LENGTH 64
#define MAX_NONCE_LENGTH 32

#ifdef HAVE_BORINGSSL
#define CIPHER_METHOD_MAP_SODIUM(XX)                       \
  XX(0x3U, AES256GCMSHA256, "aes-256-gcm")                 \
  XX(0x4U, CHACHA20POLY1305IETF, "chacha20-ietf-poly1305") \
  XX(0x5U, XCHACHA20POLY1305IETF, "xchacha20-ietf-poly1305")
#else
#define CIPHER_METHOD_MAP_SODIUM(XX)
#endif

#ifdef HAVE_BORINGSSL
#define CIPHER_METHOD_MAP_BORINGSSL(XX)                               \
  XX(0x14U, CHACHA20POLY1305IETF_EVP, "chacha20-ietf-poly1305-evp")   \
  XX(0x15U, XCHACHA20POLY1305IETF_EVP, "xchacha20-ietf-poly1305-evp") \
  XX(0x16U, AES128GCMSHA256_EVP, "aes-128-gcm-evp")                   \
  XX(0x17U, AES128GCM12SHA256_EVP, "aes-128-gcm12-evp")               \
  XX(0x18U, AES192GCMSHA256_EVP, "aes-192-gcm-evp")                   \
  XX(0x19U, AES256GCMSHA256_EVP, "aes-256-gcm-evp")
#else
#define CIPHER_METHOD_MAP_BORINGSSL(XX)
#endif

#ifdef HAVE_QUICHE
#define CIPHER_METHOD_MAP_HTTP2(XX)                                   \
  XX(0x20U, HTTP2, "http2-protocol")                                  \
  XX(0x21U, HTTP2_INPLACE_1, "http2-1-protocol")                      \
  XX(0x22U, HTTP2_INPLACE_2, "http2-2-protocol")                      \
  XX(0x23U, HTTP2_INPLACE_3, "http2-3-protocol")                      \
  XX(0x24U, HTTP2_INPLACE_4, "http2-4-protocol")                      \
  XX(0x25U, HTTP2_INPLACE_5, "http2-5-protocol")
#else
#define CIPHER_METHOD_MAP_HTTP2(XX)
#endif

#define CIPHER_METHOD_OLD_MAP(XX) \
  CIPHER_METHOD_MAP_SODIUM(XX)      \
  CIPHER_METHOD_MAP_BORINGSSL(XX)

#define CIPHER_METHOD_VALID_MAP(XX) \
  CIPHER_METHOD_MAP_SODIUM(XX)      \
  CIPHER_METHOD_MAP_BORINGSSL(XX)   \
  CIPHER_METHOD_MAP_HTTP2(XX)

#define CIPHER_METHOD_MAP(XX)  \
  XX(0x0U, INVALID, "invalid") \
  CIPHER_METHOD_VALID_MAP(XX)

enum cipher_method : uint32_t {
#define XX(num, name, string) CRYPTO_##name = num,
  CIPHER_METHOD_MAP(XX)
#undef XX
};

enum cipher_method to_cipher_method(const std::string& method);
const char* to_cipher_method_str(enum cipher_method method);
bool is_valid_cipher_method(enum cipher_method method);

#define XX(num, name, string) extern const char* CRYPTO_##name##_STR;
CIPHER_METHOD_MAP(XX)
#undef XX

#endif  // H_CRYPTO_CRYPTER_EXPORT
