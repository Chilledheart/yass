// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "crypto/encrypter.hpp"

#include "crypto/aes_128_gcm_12_evp_encrypter.hpp"
#include "crypto/aes_128_gcm_evp_encrypter.hpp"
#include "crypto/aes_192_gcm_evp_encrypter.hpp"
#include "crypto/aes_256_gcm_evp_encrypter.hpp"
#include "crypto/aes_256_gcm_sodium_encrypter.hpp"
#include "crypto/chacha20_poly1305_evp_encrypter.hpp"
#include "crypto/chacha20_poly1305_sodium_encrypter.hpp"
#include "crypto/crypter_export.hpp"
#include "crypto/xchacha20_poly1305_evp_encrypter.hpp"
#include "crypto/xchacha20_poly1305_sodium_encrypter.hpp"

namespace crypto {

Encrypter::~Encrypter() = default;

std::unique_ptr<Encrypter> Encrypter::CreateFromCipherSuite(
    uint32_t cipher_suite) {
  switch (cipher_suite) {
#ifdef HAVE_BORINGSSL
    case CRYPTO_AES256GCMSHA256:
      return std::make_unique<Aes256GcmSodiumEncrypter>();
    case CRYPTO_CHACHA20POLY1305IETF:
      return std::make_unique<ChaCha20Poly1305SodiumEncrypter>();
    case CRYPTO_XCHACHA20POLY1305IETF:
      return std::make_unique<XChaCha20Poly1305SodiumEncrypter>();
    case CRYPTO_CHACHA20POLY1305IETF_EVP:
      return std::make_unique<ChaCha20Poly1305EvpEncrypter>();
    case CRYPTO_XCHACHA20POLY1305IETF_EVP:
      return std::make_unique<XChaCha20Poly1305EvpEncrypter>();
    case CRYPTO_AES128GCMSHA256_EVP:
      return std::make_unique<Aes128GcmEvpEncrypter>();
    case CRYPTO_AES128GCM12SHA256_EVP:
      return std::make_unique<Aes128Gcm12EvpEncrypter>();
    case CRYPTO_AES192GCMSHA256_EVP:
      return std::make_unique<Aes192GcmEvpEncrypter>();
    case CRYPTO_AES256GCMSHA256_EVP:
      return std::make_unique<Aes256GcmEvpEncrypter>();
#endif
    default:
      return nullptr;
  }
}

}  // namespace crypto
