// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_CRYPTO_AEAD_MBEDTLS_ENCRYPTER
#define H_CRYPTO_AEAD_MBEDTLS_ENCRYPTER

#include "crypto/aead_base_encrypter.hpp"
#include "crypto/crypter_export.hpp"

#ifdef HAVE_MBEDTLS

#include <mbedtls/cipher.h>

namespace crypto {

/// actually, it is stream cipher, not ahead cipher
class AeadMbedtlsEncrypter : public AeadBaseEncrypter {
 public:
  AeadMbedtlsEncrypter(cipher_method method,
                       mbedtls_cipher_context_t*,
                       size_t key_size,
                       size_t auth_tag_size,
                       size_t nonce_size);
  ~AeadMbedtlsEncrypter() override;

  uint32_t cipher_id() const override {
    return method_;
  }

  bool SetKey(const char* key, size_t key_len) override;

  bool EncryptPacket(uint64_t packet_number,
                     const char* associated_data,
                     size_t associated_data_len,
                     const char* plaintext,
                     size_t plaintext_len,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override;

  bool Encrypt(const uint8_t* nonce,
               size_t nonce_len,
               const char* associated_data,
               size_t associated_data_len,
               const char* plaintext,
               size_t plaintext_len,
               uint8_t* output,
               size_t* output_length,
               size_t max_output_length);

 private:
  cipher_method method_;
  mbedtls_cipher_context_t* evp_;
};

}  // namespace crypto

#endif

#endif  // H_CRYPTO_AEAD_MBEDTLS_ENCRYPTER
