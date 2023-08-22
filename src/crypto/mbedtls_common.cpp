// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifdef HAVE_MBEDTLS

#include "crypto/mbedtls_common.hpp"
#include "core/logging.hpp"

namespace crypto {

mbedtls_cipher_context_t* mbedtls_create_evp(enum cipher_method method) {
  const mbedtls_cipher_info_t* info = mbedtls_get_cipher(method);
  if (!info) {
    return nullptr;
  }
  auto *evp = new mbedtls_cipher_context_t;
  mbedtls_cipher_init(evp);
  if (mbedtls_cipher_setup(evp, info) != 0) {
    LOG(WARNING) << "mbedtls: setup failed";
    delete evp;
    return nullptr;
  }
  return evp;
}

void mbedtls_release_evp(mbedtls_cipher_context_t* evp) {
  mbedtls_cipher_free(evp);
}

const mbedtls_cipher_info_t* mbedtls_get_cipher(enum cipher_method method) {
  const char* name = nullptr;
  //  "table",
  //  "ARC4-128",
  //  "ARC4-128",
  //  "AES-128-CFB128",
  //  "AES-192-CFB128",
  //  "AES-256-CFB128",
  //  "AES-128-CTR",
  //  "AES-192-CTR",
  //  "AES-256-CTR",
  //  "BLOWFISH-CFB64",
  //  "CAMELLIA-128-CFB128",
  //  "CAMELLIA-192-CFB128",
  //  "CAMELLIA-256-CFB128",
  //  "salsa20",
  //  "chacha20",
  //  "chacha20-ietf"
  switch(method) {
    case CRYPTO_RC4:
    case CRYPTO_RC4_MD5:
      name = "ARC4-128";
      break;
    case CRYPTO_AES_128_CFB:
      name = "AES-128-CFB128";
      break;
    case CRYPTO_AES_192_CFB:
      name = "AES-192-CFB128";
      break;
    case CRYPTO_AES_256_CFB:
      name = "AES-256-CFB128";
      break;
    case CRYPTO_AES_128_CTR:
      name = "AES-128-CTR";
      break;
    case CRYPTO_AES_192_CTR:
      name = "AES-192-CTR";
      break;
    case CRYPTO_AES_256_CTR:
      name = "AES-256-CTR";
      break;
    case CRYPTO_BF_CFB:
      name = "BLOWFISH-CFB64";
      break;
    case CRYPTO_CAMELLIA_128_CFB:
      name = "CAMELLIA-128-CFB128";
      break;
    case CRYPTO_CAMELLIA_192_CFB:
      name = "CAMELLIA-192-CFB128";
      break;
    case CRYPTO_CAMELLIA_256_CFB:
      name = "CAMELLIA-256-CFB128";
      break;
    case CRYPTO_SALSA20:
      name = "sala20";
      break;
    case CRYPTO_CHACHA20:
      name = "chacha20";
      break;
    case CRYPTO_CHACHA20IETF:
      name = "chacha20-ietf";
      break;
    default:
      LOG(WARNING) << "bad cipher method: " << method;
      name = nullptr;
      break;
  }
  if (!name) {
    return nullptr;
  }
  return mbedtls_cipher_info_from_string(name);
}

uint8_t mbedtls_get_nonce_size(enum cipher_method method) {
  // TABLE, RC4, ..., CHACHA20_IETF
  // 0, 0, 16, 16, 16, 16, 16, 16, 16, 8, 16, 16, 16, 8, 8, 12
  switch(method) {
    case CRYPTO_RC4:
      return 0;
    case CRYPTO_RC4_MD5:
    case CRYPTO_AES_128_CFB:
    case CRYPTO_AES_192_CFB:
    case CRYPTO_AES_256_CFB:
    case CRYPTO_AES_128_CTR:
    case CRYPTO_AES_192_CTR:
    case CRYPTO_AES_256_CTR:
      return 16;
    case CRYPTO_BF_CFB:
      return 8;
    case CRYPTO_CAMELLIA_128_CFB:
    case CRYPTO_CAMELLIA_192_CFB:
    case CRYPTO_CAMELLIA_256_CFB:
      return 16;
    case CRYPTO_SALSA20:
    case CRYPTO_CHACHA20:
      return 8;
    case CRYPTO_CHACHA20IETF:
      return 12;
    default:
      LOG(WARNING) << "bad cipher method: " << method;
      return -1;
  }
}

uint8_t mbedtls_get_key_size(enum cipher_method method) {
  // TABLE, RC4, ..., CHACHA20_IETF
  // 0, 16, 16, 16, 24, 32, 16, 24, 32, 16, 16, 24, 32, 32, 32, 32
  switch(method) {
    case CRYPTO_RC4:
    case CRYPTO_RC4_MD5:
      return 16;
    case CRYPTO_AES_128_CFB:
      return 16;
    case CRYPTO_AES_192_CFB:
      return 24;
    case CRYPTO_AES_256_CFB:
      return 32;
    case CRYPTO_AES_128_CTR:
      return 16;
    case CRYPTO_AES_192_CTR:
      return 24;
    case CRYPTO_AES_256_CTR:
      return 32;
    case CRYPTO_BF_CFB:
      return 16;
    case CRYPTO_CAMELLIA_128_CFB:
      return 16;
    case CRYPTO_CAMELLIA_192_CFB:
      return 24;
    case CRYPTO_CAMELLIA_256_CFB:
      return 32;
    case CRYPTO_SALSA20:
    case CRYPTO_CHACHA20:
    case CRYPTO_CHACHA20IETF:
      return 32;
    default:
      LOG(WARNING) << "bad cipher method: " << method;
      return -1;
  }
}

} // namespace crypto

#endif // HAVE_MBEDTLS
