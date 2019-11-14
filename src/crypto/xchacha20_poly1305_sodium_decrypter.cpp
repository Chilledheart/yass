//
// xchacha20_poly1305_sodium_decrypter.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "crypto/xchacha20_poly1305_sodium_decrypter.hpp"

#include "core/cipher.hpp"
#include <glog/logging.h>
#include <sodium/crypto_aead_xchacha20poly1305.h>

static const size_t kKeySize = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
static const size_t kNonceSize = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;

namespace crypto {

XChaCha20Poly1305SodiumDecrypter::XChaCha20Poly1305SodiumDecrypter()
    : AeadBaseDecrypter(kKeySize, kAuthTagSize, kNonceSize) {}

XChaCha20Poly1305SodiumDecrypter::~XChaCha20Poly1305SodiumDecrypter() {}

bool XChaCha20Poly1305SodiumDecrypter::DecryptPacket(
                                     uint64_t packet_number,
                                     const char *associated_data,
                                     size_t associated_data_len,
                                     const char *ciphertext,
                                     size_t ciphertext_len,
                                     char *output,
                                     size_t *output_length,
                                     size_t max_output_length) {
  unsigned long long plaintext_size;
  if (ciphertext_len < auth_tag_size_) {
    return false;
  }

  if (have_preliminary_key_) {
    LOG(ERROR) << "Unable to decrypt while key diversification is pending";
    return false;
  }

  uint8_t nonce[kMaxNonceSize];
  memcpy(nonce, iv_, nonce_size_);
  size_t prefix_len = nonce_size_ - sizeof(packet_number);
  memcpy(nonce + prefix_len, &packet_number, sizeof(packet_number));

  if (::crypto_aead_xchacha20poly1305_ietf_decrypt(
          reinterpret_cast<uint8_t *>(output), &plaintext_size, nullptr,
          reinterpret_cast<const uint8_t *>(ciphertext), ciphertext_len,
          reinterpret_cast<const uint8_t *>(associated_data), associated_data_len,
          reinterpret_cast<const uint8_t *>(nonce),
          reinterpret_cast<const uint8_t *>(key_)) != 0) {
    return false;
  }
  *output_length = plaintext_size;
  return true;
}

uint32_t XChaCha20Poly1305SodiumDecrypter::cipher_id() const {
  return CRYPTO_CHACHA20POLY1305IETF;
}

} // namespace crypto
