// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "crypto/xchacha20_poly1305_sodium_encrypter.hpp"

#ifdef HAVE_LIBSODIUM

#include <sodium/crypto_aead_xchacha20poly1305.h>
#include "core/logging.hpp"

#include "core/cipher.hpp"

static const size_t kKeySize = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
static const size_t kNonceSize = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;

namespace crypto {

XChaCha20Poly1305SodiumEncrypter::XChaCha20Poly1305SodiumEncrypter()
    : AeadBaseEncrypter(kKeySize, kAuthTagSize, kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

XChaCha20Poly1305SodiumEncrypter::~XChaCha20Poly1305SodiumEncrypter() = default;

bool XChaCha20Poly1305SodiumEncrypter::EncryptPacket(
    uint64_t packet_number,
    const char* associated_data,
    size_t associated_data_len,
    const char* plaintext,
    size_t plaintext_len,
    char* output,
    size_t* output_length,
    size_t max_output_length) {
  unsigned long long ciphertext_size = GetCiphertextSize(plaintext_len);
  if (max_output_length < ciphertext_size) {
    return false;
  }
  if (associated_data || associated_data_len) {
    return false;
  }
  uint8_t nonce[kMaxNonceSize];
  memcpy(nonce, iv_, nonce_size_);

  // for libsodium, packet number is written ahead
  PacketNumberToNonce(nonce, packet_number);

  if (::crypto_aead_xchacha20poly1305_ietf_encrypt(
          reinterpret_cast<uint8_t*>(output), &ciphertext_size,
          reinterpret_cast<const uint8_t*>(plaintext), plaintext_len,
          reinterpret_cast<const uint8_t*>(associated_data),
          associated_data_len, nullptr, nonce,
          reinterpret_cast<const uint8_t*>(key_)) != 0) {
    return false;
  }
  *output_length = ciphertext_size;
  return true;
}

uint32_t XChaCha20Poly1305SodiumEncrypter::cipher_id() const {
  return CRYPTO_XCHACHA20POLY1305IETF;
}

}  // namespace crypto

#endif  // HAVE_LIBSODIUM
