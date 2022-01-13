// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef H_CRYPTO_CHACHA20_POLY1305_SODIUM_ENCRYPTER
#define H_CRYPTO_CHACHA20_POLY1305_SODIUM_ENCRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_sodium_encrypter.hpp"

#ifdef HAVE_BORINGSSL

namespace crypto {

class ChaCha20Poly1305SodiumEncrypter : public SodiumAeadEncrypter {
 public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  ChaCha20Poly1305SodiumEncrypter();
  ~ChaCha20Poly1305SodiumEncrypter() override;

  uint32_t cipher_id() const override;
};

}  // namespace crypto

#endif

#endif  // H_CRYPTO_CHACHA20_POLY1305_SODIUM_ENCRYPTER
