// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CRYPTO_AES256_GCM_EVP_DECRYPTER
#define H_CRYPTO_AES256_GCM_EVP_DECRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_evp_decrypter.hpp"

namespace crypto {

class Aes256GcmEvpDecrypter : public AeadEvpDecrypter {
 public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  Aes256GcmEvpDecrypter();
  ~Aes256GcmEvpDecrypter() override;

  uint32_t cipher_id() const override;
};

}  // namespace crypto

#endif  // H_CRYPTO_AES256_GCM_EVP_DECRYPTER
