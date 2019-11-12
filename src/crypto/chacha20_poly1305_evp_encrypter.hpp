//
// chacha20_poly1305_evp_encrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CRYPTO_CHACHA20_POLY1305_EVP_ENCRYPTER
#define H_CRYPTO_CHACHA20_POLY1305_EVP_ENCRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_evp_encrypter.hpp"

#ifdef HAVE_BORINGSSL

namespace crypto {

class ChaCha20Poly1305EvpEncrypter : public EvpAeadEncrypter {
public:
  enum : size_t {
    kAuthTagSize = 12,
  };

  ChaCha20Poly1305EvpEncrypter();
  ~ChaCha20Poly1305EvpEncrypter() override;

  uint32_t cipher_id() const override;
};

} // namespace crypto

#endif

#endif // H_CRYPTO_CHACHA20_POLY1305_EVP_ENCRYPTER
