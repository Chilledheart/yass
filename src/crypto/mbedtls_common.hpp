// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_CRYPTO_MEBDTLS_COMMON
#define H_CRYPTO_MEBDTLS_COMMON

#ifdef HAVE_MBEDTLS
#include "crypto/crypter_export.hpp"

#include <mbedtls/cipher.h>
#include <mbedtls/version.h>

namespace crypto {
mbedtls_cipher_context_t* mbedtls_create_evp(enum cipher_method method);
void mbedtls_release_evp(mbedtls_cipher_context_t* evp);
const mbedtls_cipher_info_t* mbedtls_get_cipher(enum cipher_method method);
uint8_t mbedtls_get_nonce_size(enum cipher_method method);
uint8_t mbedtls_get_key_size(enum cipher_method method);
}  // namespace crypto

#endif  // HAVE_MBEDTLS
#endif  // H_CRYPTO_MEBDTLS_COMMON
