//
// crypter_export.hpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CRYPTO_CRYPTER_EXPORT
#define H_CRYPTO_CRYPTER_EXPORT

#include <stddef.h>
#include <stdint.h>
#include <string>

#define MAX_KEY_LENGTH 64
#define MAX_NONCE_LENGTH 32

enum cipher_method : uint32_t {
  PLAINTEXT = 0x0,
  /* UNUSED = 0x1 */
  /* UNUSED = 0x2 */
  /* UNUSED = 0x3 */
  CHACHA20POLY1305IETF = 0x4,
  XCHACHA20POLY1305IETF = 0x5,
#ifdef HAVE_BORINGSSL
  CHACHA20POLY1305IETF_EVP = 0x14,
  XCHACHA20POLY1305IETF_EVP = 0x15,
#endif
  AES128GCMSHA256_EVP = 0x16,
  AES128GCM12SHA256_EVP = 0x17,
  AES192GCMSHA256_EVP = 0x18,
  AES256GCMSHA256_EVP = 0x19,
  MAX_CIPHER_METHOD,
};

enum cipher_method to_cipher_method(const std::string &method);
const char *to_cipher_method_str(enum cipher_method method);

#endif // H_CRYPTO_CRYPTER_EXPORT
