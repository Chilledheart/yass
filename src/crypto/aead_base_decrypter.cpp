// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "aead_base_decrypter.hpp"

#include <cstring>

#include "core/logging.hpp"

namespace crypto {

AeadBaseDecrypter::AeadBaseDecrypter(size_t key_size,
                                     size_t auth_tag_size,
                                     size_t nonce_size)
    : key_size_(key_size),
      auth_tag_size_(auth_tag_size),
      nonce_size_(nonce_size),
      have_preliminary_key_(false) {
  DCHECK_LE(key_size_, sizeof(key_));
  DCHECK_LE(nonce_size_, sizeof(iv_));
  DCHECK_GE(kMaxNonceSize, nonce_size_);

  memset(key_, 0, kMaxKeySize);
  memset(iv_, 0, kMaxNonceSize);
}

AeadBaseDecrypter::~AeadBaseDecrypter() = default;

bool AeadBaseDecrypter::SetKey(const char* key, size_t key_len) {
  DCHECK_EQ(key_len, key_size_);
  if (key_len != key_size_) {
    return false;
  }
  memcpy(key_, key, key_len);
  return true;
}

bool AeadBaseDecrypter::SetNoncePrefix(const char* nonce_prefix,
                                       size_t nonce_prefix_len) {
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

size_t AeadBaseDecrypter::GetKeySize() const {
  return key_size_;
}

size_t AeadBaseDecrypter::GetNoncePrefixSize() const {
  return nonce_size_ - sizeof(uint64_t);
}

size_t AeadBaseDecrypter::GetIVSize() const {
  return nonce_size_;
}

size_t AeadBaseDecrypter::GetTagSize() const {
  return auth_tag_size_;
}

const uint8_t* AeadBaseDecrypter::GetKey() const {
  return key_;
}

const uint8_t* AeadBaseDecrypter::GetIV() const {
  return iv_;
}

const uint8_t* AeadBaseDecrypter::GetNoncePrefix() const {
  return iv_;
}

}  // namespace crypto
