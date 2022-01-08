// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CRYPTO_AEAD_BASE_ENCRYPTER
#define H_CRYPTO_AEAD_BASE_ENCRYPTER

#include "crypto/encrypter.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace crypto {

class AeadBaseEncrypter : public Encrypter {
 public:
  AeadBaseEncrypter(size_t key_size, size_t auth_tag_size, size_t nonce_size);
  virtual ~AeadBaseEncrypter();

  bool SetKey(const char* key, size_t key_len) override;
  bool SetNoncePrefix(const char* nonce_prefix,
                      size_t nonce_prefix_len) override;
  bool SetIV(const char* iv, size_t iv_len) override;

  size_t GetKeySize() const override;
  size_t GetNoncePrefixSize() const override;
  size_t GetIVSize() const override;
  size_t GetMaxPlaintextSize(size_t ciphertext_size) const override;
  size_t GetCiphertextSize(size_t plaintext_size) const override;
  size_t GetTagSize() const override;

  const uint8_t* GetKey() const override;
  const uint8_t* GetNoncePrefix() const override;

 protected:
  static const size_t kMaxKeySize = 64;
  enum : size_t { kMaxNonceSize = 32 };

  const size_t key_size_;
  const size_t auth_tag_size_;
  const size_t nonce_size_;

  // The Key.
  uint8_t key_[kMaxKeySize];
  // The IV used to construct the nonce.
  uint8_t iv_[kMaxNonceSize];
};

}  // namespace crypto

#endif  // H_CRYPTO_AEAD_BASE_ENCRYPTER
