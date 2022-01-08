// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "crypto/decrypter.hpp"

#include "crypto/aes_128_gcm_12_evp_decrypter.hpp"
#include "crypto/aes_128_gcm_evp_decrypter.hpp"
#include "crypto/aes_192_gcm_evp_decrypter.hpp"
#include "crypto/aes_256_gcm_evp_decrypter.hpp"
#include "crypto/aes_256_gcm_sodium_decrypter.hpp"
#include "crypto/chacha20_poly1305_evp_decrypter.hpp"
#include "crypto/chacha20_poly1305_sodium_decrypter.hpp"
#include "crypto/crypter_export.hpp"
#include "crypto/xchacha20_poly1305_evp_decrypter.hpp"
#include "crypto/xchacha20_poly1305_sodium_decrypter.hpp"

namespace crypto {

Decrypter::~Decrypter() = default;

std::unique_ptr<Decrypter> Decrypter::CreateFromCipherSuite(
    uint32_t cipher_suite) {
  switch (cipher_suite) {
#ifdef HAVE_LIBSODIUM
    case CRYPTO_AES256GCMSHA256:
      return std::make_unique<Aes256GcmSodiumDecrypter>();
    case CRYPTO_CHACHA20POLY1305IETF:
      return std::make_unique<ChaCha20Poly1305SodiumDecrypter>();
    case CRYPTO_XCHACHA20POLY1305IETF:
      return std::make_unique<XChaCha20Poly1305SodiumDecrypter>();
#endif
#ifdef HAVE_BORINGSSL
    case CRYPTO_CHACHA20POLY1305IETF_EVP:
      return std::make_unique<ChaCha20Poly1305EvpDecrypter>();
    case CRYPTO_XCHACHA20POLY1305IETF_EVP:
      return std::make_unique<XChaCha20Poly1305EvpDecrypter>();
    case CRYPTO_AES128GCMSHA256_EVP:
      return std::make_unique<Aes128GcmEvpDecrypter>();
    case CRYPTO_AES128GCM12SHA256_EVP:
      return std::make_unique<Aes128Gcm12EvpDecrypter>();
    case CRYPTO_AES192GCMSHA256_EVP:
      return std::make_unique<Aes192GcmEvpDecrypter>();
    case CRYPTO_AES256GCMSHA256_EVP:
      return std::make_unique<Aes256GcmEvpDecrypter>();
#endif
    default:
      return nullptr;
  }
}

#if 0
// static
void QuicDecrypter::DiversifyPreliminaryKey(QuicStringPiece preliminary_key,
                                            QuicStringPiece nonce_prefix,
                                            const DiversificationNonce& nonce,
                                            size_t key_size,
                                            size_t nonce_prefix_size,
                                            std::string* out_key,
                                            std::string* out_nonce_prefix) {
  QuicHKDF hkdf((std::string(preliminary_key)) + (std::string(nonce_prefix)),
                QuicStringPiece(nonce.data(), nonce.size()),
                "QUIC key diversification", 0, key_size, 0, nonce_prefix_size,
                0);
  *out_key = std::string(hkdf.server_write_key());
  *out_nonce_prefix = std::string(hkdf.server_write_iv());
}
#endif

}  // namespace crypto
