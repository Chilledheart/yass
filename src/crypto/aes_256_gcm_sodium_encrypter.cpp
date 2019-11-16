//
// aes_256_gcm_sodium_encrypter.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "crypto/aes_256_gcm_sodium_encrypter.hpp"

#include "core/cipher.hpp"
#include <glog/logging.h>
#include <sodium/crypto_aead_aes256gcm.h>
#include <sodium/utils.h>

static const size_t kKeySize = crypto_aead_aes256gcm_KEYBYTES;
static const size_t kNonceSize = crypto_aead_aes256gcm_NPUBBYTES;

namespace crypto {

static_assert(Aes256GcmSodiumEncrypter::kAuthTagSize == crypto_aead_aes256gcm_ABYTES,
    "mismatched AES-256-GCM auth tag size");

Aes256GcmSodiumEncrypter::Aes256GcmSodiumEncrypter()
    : AeadBaseEncrypter(kKeySize, kAuthTagSize, kNonceSize),
      ctx_(new crypto_aead_aes256gcm_state) {
  ::sodium_memzero(ctx_, sizeof(*ctx_));
}

Aes256GcmSodiumEncrypter::~Aes256GcmSodiumEncrypter() {
  delete ctx_;
}

bool Aes256GcmSodiumEncrypter::SetKey(const char* key, size_t key_len) {
  if (!AeadBaseEncrypter::SetKey(key, key_len)) {
    return false;
  }
  if (::crypto_aead_aes256gcm_beforenm(ctx_, key_) != 0) {
    return false;
  }
  return true;
}

bool Aes256GcmSodiumEncrypter::EncryptPacket(
                     uint64_t packet_number,
                     const char *associated_data,
                     size_t associated_data_len,
                     const char *plaintext,
                     size_t plaintext_len,
                     char *output,
                     size_t *output_length,
                     size_t max_output_length) {
  unsigned long long ciphertext_size = GetCiphertextSize(plaintext_len);
  if (max_output_length < ciphertext_size) {
    return false;
  }
  if (associated_data || associated_data_len) {
    return false;
  }
  char nonce_buffer[kMaxNonceSize];
  memcpy(nonce_buffer, iv_, nonce_size_);
#if 0
  size_t prefix_len = nonce_size_ - sizeof(uint64_t);
  memcpy(nonce_buffer + prefix_len, &packet_number, sizeof(uint64_t));
#endif

  if (::crypto_aead_aes256gcm_encrypt_afternm(
          reinterpret_cast<uint8_t *>(output), &ciphertext_size,
          reinterpret_cast<const uint8_t *>(plaintext), plaintext_len,
          reinterpret_cast<const uint8_t *>(associated_data), associated_data_len,
          nullptr,
          reinterpret_cast<const uint8_t *>(nonce_buffer),
          ctx_) != 0) {
    return false;
  }
  *output_length = ciphertext_size;
  return true;
}

uint32_t Aes256GcmSodiumEncrypter::cipher_id() const {
  return CRYPTO_AES256GCMSHA256;
}

} // namespace crypto
