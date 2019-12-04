//
// chacha20_poly1305_sodium_decrypter.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "crypto/chacha20_poly1305_sodium_decrypter.hpp"

#ifdef HAVE_LIBSODIUM

#include "core/logging.hpp"
#include <sodium/crypto_aead_chacha20poly1305.h>

#include "core/cipher.hpp"

static const size_t kKeySize = crypto_aead_chacha20poly1305_ietf_KEYBYTES;
static const size_t kNonceSize = crypto_aead_chacha20poly1305_ietf_NPUBBYTES;

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

ChaCha20Poly1305SodiumDecrypter::ChaCha20Poly1305SodiumDecrypter()
    : AeadBaseDecrypter(kKeySize, kAuthTagSize, kNonceSize) {}

ChaCha20Poly1305SodiumDecrypter::~ChaCha20Poly1305SodiumDecrypter() {}

bool ChaCha20Poly1305SodiumDecrypter::DecryptPacket(
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

  // for libsodium, packet number is written ahead
  PacketNumberToNonce(nonce, packet_number);

  if (::crypto_aead_chacha20poly1305_ietf_decrypt(
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

uint32_t ChaCha20Poly1305SodiumDecrypter::cipher_id() const {
  return CRYPTO_CHACHA20POLY1305IETF;
}

} // namespace crypto

#endif // HAVE_LIBSODIUM
