// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef H_CRYPTO_AES256_GCM_SODIUM_DECRYPTER
#define H_CRYPTO_AES256_GCM_SODIUM_DECRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_sodium_decrypter.hpp"

#ifdef HAVE_BORINGSSL

namespace crypto {

class Aes256GcmSodiumDecrypter : public AeadSodiumDecrypter {
 public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  Aes256GcmSodiumDecrypter();
  ~Aes256GcmSodiumDecrypter() override;

  uint32_t cipher_id() const override;
};

}  // namespace crypto

#endif

#endif  // H_CRYPTO_AES256_GCM_SODIUM_DECRYPTER
