// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#ifndef H_CRYPTO_AES256_GCM_SODIUM_ENCRYPTER
#define H_CRYPTO_AES256_GCM_SODIUM_ENCRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_sodium_encrypter.hpp"

namespace crypto {

class Aes256GcmSodiumEncrypter : public SodiumAeadEncrypter {
 public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  Aes256GcmSodiumEncrypter();
  ~Aes256GcmSodiumEncrypter() override;

  uint32_t cipher_id() const override;
};

}  // namespace crypto

#endif  // H_CRYPTO_AES256_GCM_SODIUM_ENCRYPTER
