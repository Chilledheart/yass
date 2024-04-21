// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CRYPTO_CRYPTER_EXPORT
#define H_CRYPTO_CRYPTER_EXPORT

#include <stddef.h>
#include <stdint.h>
#include <string_view>

#define MAX_KEY_LENGTH 64
#define MAX_NONCE_LENGTH 32

#define CIPHER_METHOD_MAP_SODIUM(XX)                       \
  XX(0x3U, AES256GCMSHA256, "aes-256-gcm")                 \
  XX(0x4U, CHACHA20POLY1305IETF, "chacha20-ietf-poly1305") \
  XX(0x5U, XCHACHA20POLY1305IETF, "xchacha20-ietf-poly1305")

#define CIPHER_METHOD_MAP_BORINGSSL(XX)                               \
  XX(0x14U, CHACHA20POLY1305IETF_EVP, "chacha20-ietf-poly1305-evp")   \
  XX(0x15U, XCHACHA20POLY1305IETF_EVP, "xchacha20-ietf-poly1305-evp") \
  XX(0x16U, AES128GCMSHA256_EVP, "aes-128-gcm-evp")                   \
  XX(0x17U, AES128GCM12SHA256_EVP, "aes-128-gcm12-evp")               \
  XX(0x18U, AES192GCMSHA256_EVP, "aes-192-gcm-evp")                   \
  XX(0x19U, AES256GCMSHA256_EVP, "aes-256-gcm-evp")

#ifdef HAVE_MBEDTLS
#define CIPHER_METHOD_MAP_MBEDTLS(XX)             \
  XX(0x22U, AES_128_CFB, "aes-128-cfb")           \
  XX(0x23U, AES_192_CFB, "aes-192-cfb")           \
  XX(0x24U, AES_256_CFB, "aes-256-cfb")           \
  XX(0x25U, AES_128_CTR, "aes-128-ctr")           \
  XX(0x26U, AES_192_CTR, "aes-192-ctr")           \
  XX(0x27U, AES_256_CTR, "aes-256-ctr")           \
  XX(0x29U, CAMELLIA_128_CFB, "camellia-128-cfb") \
  XX(0x30U, CAMELLIA_192_CFB, "camellia-192-cfb") \
  XX(0x31U, CAMELLIA_256_CFB, "camellia-256-cfb")
#else
#define CIPHER_METHOD_MAP_MBEDTLS(XX)
#endif

#define CIPHER_METHOD_MAP_HTTP(XX) XX(0x110U, HTTPS, "https")

#ifdef HAVE_QUICHE
#define CIPHER_METHOD_MAP_HTTP2(XX)              \
  XX(0x120U, HTTP2_PLAINTEXT, "http2-plaintext") \
  XX(0x121U, HTTP2, "http2")
#define CIPHER_METHOD_MAP_FULL_HTTP2(XX)          \
  XX(0x120U, HTTP2_PLAINTEXT, "http2-plaintext")  \
  XX(0x121U, HTTP2, "http2")                      \
  XX(0x122U, HTTP2_INPLACE_2, "http2-2-protocol") \
  XX(0x123U, HTTP2_INPLACE_3, "http2-3-protocol") \
  XX(0x124U, HTTP2_INPLACE_4, "http2-4-protocol") \
  XX(0x125U, HTTP2_INPLACE_5, "http2-5-protocol")
#else
#define CIPHER_METHOD_MAP_HTTP2(XX)
#endif

#ifdef HAVE_QUICHE
#define CIPHER_METHOD_IS_HTTP2(m) ((m) == CRYPTO_HTTP2_PLAINTEXT || (m) == CRYPTO_HTTP2)
#define CIPHER_METHOD_IS_TLS(m) ((m) == CRYPTO_HTTPS || (m) == CRYPTO_HTTP2)
#define CIPHER_METHOD_IS_HTTPS_FALLBACK(m) ((m) == CRYPTO_HTTPS)
#else
#define CIPHER_METHOD_IS_HTTP2(m) false
#define CIPHER_METHOD_IS_TLS(m) ((m) == CRYPTO_HTTPS)
#define CIPHER_METHOD_IS_HTTPS_FALLBACK(m) ((m) == CRYPTO_HTTPS)
#endif

#ifdef HAVE_QUICHE
#define CRYPTO_DEFAULT CRYPTO_HTTP2
#define CRYPTO_DEFAULT_STR CRYPTO_HTTP2_STR
#define CRYPTO_DEFAULT_CSTR CRYPTO_HTTP2_CSTR
#elif defined(HAVE_MBEDTLS)
#define CRYPTO_DEFAULT CHACHA20POLY1305IETF_EVP
#define CRYPTO_DEFAULT_STR CHACHA20POLY1305IETF_EVP_STR
#define CRYPTO_DEFAULT_CSTR CHACHA20POLY1305IETF_EVP_CSTR
#else
#define CRYPTO_DEFAULT CRYPTO_AES256GCMSHA256
#define CRYPTO_DEFAULT_STR CRYPTO_AES256GCMSHA256_STR
#define CRYPTO_DEFAULT_CSTR CRYPTO_AES256GCMSHA256_CSTR
#endif

#define CIPHER_METHOD_OLD_MAP(XX) \
  CIPHER_METHOD_MAP_SODIUM(XX)    \
  CIPHER_METHOD_MAP_BORINGSSL(XX) \
  CIPHER_METHOD_MAP_MBEDTLS(XX)

#define CIPHER_METHOD_VALID_MAP(XX) \
  CIPHER_METHOD_MAP_SODIUM(XX)      \
  CIPHER_METHOD_MAP_BORINGSSL(XX)   \
  CIPHER_METHOD_MAP_MBEDTLS(XX)     \
  CIPHER_METHOD_MAP_HTTP(XX)        \
  CIPHER_METHOD_MAP_HTTP2(XX)

#define CIPHER_METHOD_MAP(XX)  \
  XX(0x0U, INVALID, "invalid") \
  CIPHER_METHOD_VALID_MAP(XX)

enum cipher_method : uint32_t {
#define XX(num, name, string) CRYPTO_##name = num,
  CIPHER_METHOD_MAP(XX)
#undef XX
};

enum cipher_method to_cipher_method(const std::string_view& method);
std::string_view to_cipher_method_name(enum cipher_method method);
std::string_view to_cipher_method_str(enum cipher_method method);
bool is_valid_cipher_method(enum cipher_method method);

#define XX(num, name, string) constexpr const std::string_view CRYPTO_##name##_STR = string;
CIPHER_METHOD_MAP(XX)
#undef XX

#define XX(num, name, string) constexpr const std::string_view CRYPTO_##name##_NAME = #name;
CIPHER_METHOD_MAP(XX)
#undef XX

extern const std::string_view kCipherMethodsStr;

#define XX(num, name, string) constexpr const char CRYPTO_##name##_CSTR[] = string;
CIPHER_METHOD_MAP(XX)
#undef XX

#endif  // H_CRYPTO_CRYPTER_EXPORT
