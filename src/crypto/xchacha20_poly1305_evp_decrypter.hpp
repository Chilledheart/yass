//
// xchacha20_poly1305_evp_decrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CRYPTO_XCHACHA20_POLY1305_EVP_DECRYPTER
#define H_CRYPTO_XCHACHA20_POLY1305_EVP_DECRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_evp_decrypter.hpp"

#ifdef HAVE_BORINGSSL

namespace crypto {

class XChaCha20Poly1305EvpDecrypter : public AeadEvpDecrypter {
public:
  enum : size_t {
    kAuthTagSize = 12,
  };

  XChaCha20Poly1305EvpDecrypter();
  ~XChaCha20Poly1305EvpDecrypter() override;

  uint32_t cipher_id() const override;
};

} // namespace crypto

#endif

#endif // H_CRYPTO_XCHACHA20_POLY1305_EVP_DECRYPTER
