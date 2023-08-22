// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_CRYPTO_AEAD_MBEDTLS_DECRYPTER
#define H_CRYPTO_AEAD_MBEDTLS_DECRYPTER

#include "crypto/aead_base_decrypter.hpp"
#include "crypto/crypter_export.hpp"

#ifdef HAVE_MBEDTLS

#include <mbedtls/cipher.h>

namespace crypto {

class AeadMbedtlsDecrypter : public AeadBaseDecrypter {
 public:
  AeadMbedtlsDecrypter(cipher_method method,
                       mbedtls_cipher_context_t* evp,
                       size_t key_size,
                       size_t auth_tag_size,
                       size_t nonce_size);
  ~AeadMbedtlsDecrypter() override;

  uint32_t cipher_id() const override {
    return method_;
  }

  bool SetKey(const char* key, size_t key_len) override;

  bool DecryptPacket(uint64_t packet_number,
                     const char* associated_data,
                     size_t associated_data_len,
                     const char* ciphertext,
                     size_t ciphertext_len,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override;

 private:
  cipher_method method_;
  mbedtls_cipher_context_t* evp_;
};

}  // namespace crypto

#endif

#endif  // H_CRYPTO_AEAD_MBEDTLS_DECRYPTER
