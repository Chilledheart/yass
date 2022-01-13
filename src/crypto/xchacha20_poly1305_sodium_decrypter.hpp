// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef H_CRYPTO_XCHACHA20_POLY1305_SODIUM_DECRYPTER
#define H_CRYPTO_XCHACHA20_POLY1305_SODIUM_DECRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_sodium_decrypter.hpp"

#ifdef HAVE_BORINGSSL

namespace crypto {

class XChaCha20Poly1305SodiumDecrypter : public AeadSodiumDecrypter {
 public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  XChaCha20Poly1305SodiumDecrypter();
  ~XChaCha20Poly1305SodiumDecrypter() override;

  uint32_t cipher_id() const override;
};

}  // namespace crypto

#endif

#endif  // H_CRYPTO_XCHACHA20_POLY1305_SODIUM_DECRYPTER
