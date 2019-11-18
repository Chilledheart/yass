//
// xchacha20_poly1305_sodium_encrypter.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "crypto/xchacha20_poly1305_sodium_encrypter.hpp"

#ifdef HAVE_LIBSODIUM

#include <glog/logging.h>
#include <sodium/crypto_aead_xchacha20poly1305.h>

#include "core/cipher.hpp"

static const size_t kKeySize = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
static const size_t kNonceSize = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;

static void PacketNumberToNonce(uint8_t *nonce, uint64_t packet_number) {
  uint8_t pn_1 = packet_number & 0xff;
  uint8_t pn_2 = (packet_number & 0xff00) >> 8;
  uint8_t pn_3 = (packet_number & 0xff0000) >> 16;
  uint8_t pn_4 = (packet_number & 0xff000000) >> 24;
  *nonce++ = pn_1;
  *nonce++ = pn_2;
  *nonce++ = pn_3;
  *nonce = pn_4;
}

namespace crypto {

XChaCha20Poly1305SodiumEncrypter::XChaCha20Poly1305SodiumEncrypter()
    : AeadBaseEncrypter(kKeySize, kAuthTagSize, kNonceSize) {}

XChaCha20Poly1305SodiumEncrypter::~XChaCha20Poly1305SodiumEncrypter() {}

bool XChaCha20Poly1305SodiumEncrypter::EncryptPacket(
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

  // for libsodium, packet number is written ahead
  PacketNumberToNonce((uint8_t*)nonce_buffer, packet_number);

  if (::crypto_aead_xchacha20poly1305_ietf_encrypt(
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

uint32_t XChaCha20Poly1305SodiumEncrypter::cipher_id() const {
  return CRYPTO_XCHACHA20POLY1305IETF;
}

} // namespace crypto

#endif // HAVE_LIBSODIUM
