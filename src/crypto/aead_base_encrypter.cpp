//
// aead_base_encrypter.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "aead_base_encrypter.hpp"

#include <sodium/randombytes.h>
#include <sodium/utils.h>

#include <glog/logging.h>

namespace crypto {

AeadBaseEncrypter::AeadBaseEncrypter(size_t key_size, size_t auth_tag_size,
                                     size_t nonce_size)
    : key_size_(key_size), auth_tag_size_(auth_tag_size),
      nonce_size_(nonce_size) {
  DCHECK_LE(key_size_, sizeof(key_));
  DCHECK_LE(nonce_size_, sizeof(iv_));
  DCHECK_GE(kMaxNonceSize, nonce_size_);

  ::sodium_memzero(key_, key_size_);
  ::sodium_memzero(iv_, kMaxNonceSize);
}

AeadBaseEncrypter::~AeadBaseEncrypter() {}

bool AeadBaseEncrypter::SetKey(const char* key, size_t key_len) {
  DCHECK_EQ(key_len, key_size_);
  if (key_len != key_size_) {
    return false;
  }
  memcpy(key_, key, key_len);
  return true;
}

bool AeadBaseEncrypter::SetNoncePrefix(const char* nonce_prefix, size_t nonce_prefix_len) {
  DCHECK_EQ(nonce_prefix_len, nonce_size_ - sizeof(uint64_t));
  if (nonce_prefix_len != nonce_size_ - sizeof(uint64_t)) {
    return false;
  }
  memcpy(iv_, nonce_prefix, nonce_prefix_len);
  return true;
}

bool AeadBaseEncrypter::SetIV(const char* iv, size_t iv_len) {
  DCHECK_EQ(iv_len, nonce_size_);
  if (iv_len != nonce_size_) {
    return false;
  }
  memcpy(iv_, iv, iv_len);
  return true;
}

size_t AeadBaseEncrypter::GetKeySize() const { return key_size_; }

size_t AeadBaseEncrypter::GetNoncePrefixSize() const {
  return nonce_size_ - sizeof(uint64_t);
}

size_t AeadBaseEncrypter::GetIVSize() const { return nonce_size_; }

size_t AeadBaseEncrypter::GetMaxPlaintextSize(size_t ciphertext_size) const {
  return ciphertext_size - std::min(ciphertext_size, auth_tag_size_);
}

size_t AeadBaseEncrypter::GetCiphertextSize(size_t plaintext_size) const {
  return plaintext_size + auth_tag_size_;
}

size_t AeadBaseEncrypter::GetTagSize() const { return auth_tag_size_; }

const uint8_t *AeadBaseEncrypter::GetKey() const { return key_; }

const uint8_t *AeadBaseEncrypter::GetNoncePrefix() const { return iv_; }

} // namespace crypto
