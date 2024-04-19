// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#ifndef H_CRYPTO_XCHACHA20_POLY1305_SODIUM_ENCRYPTER
#define H_CRYPTO_XCHACHA20_POLY1305_SODIUM_ENCRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_sodium_encrypter.hpp"

namespace crypto {

class XChaCha20Poly1305SodiumEncrypter : public SodiumAeadEncrypter {
 public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  XChaCha20Poly1305SodiumEncrypter();
  ~XChaCha20Poly1305SodiumEncrypter() override;

  uint32_t cipher_id() const override;
};

}  // namespace crypto

#endif  // H_CRYPTO_XCHACHA20_POLY1305_SODIUM_ENCRYPTER
