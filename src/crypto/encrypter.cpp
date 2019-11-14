//
// encrypter.cpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "crypto/encrypter.hpp"

#include "crypto/crypter_export.hpp"
#include "crypto/aes_128_gcm_12_evp_encrypter.hpp"
#include "crypto/aes_128_gcm_evp_encrypter.hpp"
#include "crypto/aes_192_gcm_evp_encrypter.hpp"
#include "crypto/aes_256_gcm_evp_encrypter.hpp"
#include "crypto/chacha20_poly1305_evp_encrypter.hpp"
#include "crypto/chacha20_poly1305_sodium_encrypter.hpp"
#include "crypto/xchacha20_poly1305_evp_encrypter.hpp"
#include "crypto/xchacha20_poly1305_sodium_encrypter.hpp"

namespace crypto {

Encrypter::~Encrypter() {}

std::unique_ptr<Encrypter> Encrypter::CreateFromCipherSuite(uint32_t cipher_suite) {
  switch (cipher_suite) {
    case CHACHA20POLY1305IETF:
      return std::make_unique<ChaCha20Poly1305SodiumEncrypter>();
    case XCHACHA20POLY1305IETF:
      return std::make_unique<XChaCha20Poly1305SodiumEncrypter>();
#ifdef HAVE_BORINGSSL
    case CHACHA20POLY1305IETF_EVP:
      return std::make_unique<ChaCha20Poly1305EvpEncrypter>();
    case XCHACHA20POLY1305IETF_EVP:
      return std::make_unique<XChaCha20Poly1305EvpEncrypter>();
#endif
    default:
      return nullptr;
  }
}

} // namespace crypto
