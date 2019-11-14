//
// aead_base_decrypter.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "aead_base_decrypter.hpp"

#include <sodium/utils.h>

#include <glog/logging.h>

namespace crypto {

AeadBaseDecrypter::AeadBaseDecrypter(size_t key_size, size_t auth_tag_size,
                                     size_t nonce_size)
    : key_size_(key_size), auth_tag_size_(auth_tag_size),
      nonce_size_(nonce_size), have_preliminary_key_(false) {
  DCHECK_LE(key_size_, sizeof(key_));
  DCHECK_LE(nonce_size_, sizeof(iv_));
  DCHECK_GE(kMaxNonceSize, nonce_size_);

  ::sodium_memzero(key_, key_size_);
  ::sodium_memzero(iv_, kMaxNonceSize);
}

AeadBaseDecrypter::~AeadBaseDecrypter() {}

bool AeadBaseDecrypter::SetKey(const char* key, size_t key_len) {
  DCHECK_EQ(key_len, key_size_);
  if (key_len != key_size_) {
    return false;
  }
  memcpy(key_, key, key_len);
  return true;
}

bool AeadBaseDecrypter::SetNoncePrefix(const char* nonce_prefix, size_t nonce_prefix_len) {
  DCHECK_EQ(nonce_prefix_len, nonce_size_ - sizeof(uint64_t));
  if (nonce_prefix_len != nonce_size_ - sizeof(uint64_t)) {
    return false;
  }
  memcpy(iv_, nonce_prefix, nonce_prefix_len);
  return true;
}

bool AeadBaseDecrypter::SetIV(const char* iv, size_t iv_len) {
  DCHECK_EQ(iv_len, nonce_size_);
  if (iv_len != nonce_size_) {
    return false;
  }
  memcpy(iv_, iv, iv_len);
  return true;
}

bool AeadBaseDecrypter::SetPreliminaryKey(const char* key, size_t key_len) {
  DCHECK(!have_preliminary_key_);
  SetKey(key, key_len);
  have_preliminary_key_ = true;

  return true;
}

size_t AeadBaseDecrypter::GetKeySize() const { return key_size_; }

size_t AeadBaseDecrypter::GetNoncePrefixSize() const {
  return nonce_size_ - sizeof(uint64_t);
}

size_t AeadBaseDecrypter::GetIVSize() const { return nonce_size_; }

size_t AeadBaseDecrypter::GetTagSize() const { return auth_tag_size_; }

const uint8_t *AeadBaseDecrypter::GetKey() const { return key_; }

const uint8_t *AeadBaseDecrypter::GetNoncePrefix() const { return iv_; }

} // namespace crypto
