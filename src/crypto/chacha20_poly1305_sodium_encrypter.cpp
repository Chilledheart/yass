//
// chacha20_poly1305_sodium_encrypter.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "crypto/chacha20_poly1305_sodium_encrypter.hpp"

#include "core/cipher.hpp"
#include <glog/logging.h>
#include <sodium/crypto_aead_chacha20poly1305.h>

static const size_t kKeySize = crypto_aead_chacha20poly1305_ietf_KEYBYTES;
static const size_t kNonceSize = crypto_aead_chacha20poly1305_ietf_NPUBBYTES;

namespace crypto {

ChaCha20Poly1305SodiumEncrypter::ChaCha20Poly1305SodiumEncrypter()
    : AeadBaseEncrypter(kKeySize, kAuthTagSize, kNonceSize) {}

ChaCha20Poly1305SodiumEncrypter::~ChaCha20Poly1305SodiumEncrypter() {}

bool ChaCha20Poly1305SodiumEncrypter::EncryptPacket(
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
  size_t prefix_len = nonce_size_ - sizeof(uint64_t);
  memcpy(nonce_buffer + prefix_len, &packet_number, sizeof(uint64_t));

  if (::crypto_aead_chacha20poly1305_ietf_encrypt(
          reinterpret_cast<uint8_t *>(output), &ciphertext_size,
          reinterpret_cast<const uint8_t *>(plaintext), plaintext_len,
          reinterpret_cast<const uint8_t *>(associated_data), associated_data_len,
          nullptr,
          reinterpret_cast<const uint8_t *>(nonce_buffer),
          reinterpret_cast<const uint8_t *>(key_)) != 0) {
    return false;
  }
  *output_length = ciphertext_size;
  return true;
}

uint32_t ChaCha20Poly1305SodiumEncrypter::cipher_id() const {
  return CHACHA20POLY1305IETF;
}

} // namespace crypto
