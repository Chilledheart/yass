// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "crypto/chacha20_poly1305_sodium_decrypter.hpp"

#ifdef HAVE_LIBSODIUM

#include <sodium/crypto_aead_chacha20poly1305.h>
#include "core/logging.hpp"

#include "core/cipher.hpp"

static const size_t kKeySize = crypto_aead_chacha20poly1305_ietf_KEYBYTES;
static const size_t kNonceSize = crypto_aead_chacha20poly1305_ietf_NPUBBYTES;

namespace crypto {

ChaCha20Poly1305SodiumDecrypter::ChaCha20Poly1305SodiumDecrypter()
    : AeadBaseDecrypter(kKeySize, kAuthTagSize, kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

ChaCha20Poly1305SodiumDecrypter::~ChaCha20Poly1305SodiumDecrypter() = default;

bool ChaCha20Poly1305SodiumDecrypter::DecryptPacket(
    uint64_t packet_number,
    const char* associated_data,
    size_t associated_data_len,
    const char* ciphertext,
    size_t ciphertext_len,
    char* output,
    size_t* output_length,
    size_t /*max_output_length*/) {
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
          reinterpret_cast<uint8_t*>(output), &plaintext_size, nullptr,
          reinterpret_cast<const uint8_t*>(ciphertext), ciphertext_len,
          reinterpret_cast<const uint8_t*>(associated_data),
          associated_data_len, reinterpret_cast<const uint8_t*>(nonce),
          reinterpret_cast<const uint8_t*>(key_)) != 0) {
    return false;
  }
  *output_length = plaintext_size;
  return true;
}

uint32_t ChaCha20Poly1305SodiumDecrypter::cipher_id() const {
  return CRYPTO_CHACHA20POLY1305IETF;
}

}  // namespace crypto

#endif  // HAVE_LIBSODIUM
