// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "crypto/aes_256_gcm_sodium_encrypter.hpp"

#ifdef HAVE_LIBSODIUM

#include <cstring>
#include <sodium/crypto_aead_aes256gcm.h>

#include "core/cipher.hpp"
#include "core/logging.hpp"

static const size_t kKeySize = crypto_aead_aes256gcm_KEYBYTES;
static const size_t kNonceSize = crypto_aead_aes256gcm_NPUBBYTES;

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

static_assert(Aes256GcmSodiumEncrypter::kAuthTagSize ==
                  crypto_aead_aes256gcm_ABYTES,
              "mismatched AES-256-GCM auth tag size");

Aes256GcmSodiumEncrypter::Aes256GcmSodiumEncrypter()
    : AeadBaseEncrypter(kKeySize, kAuthTagSize, kNonceSize),
      ctx_(new crypto_aead_aes256gcm_state) {
  memset(ctx_, 0, sizeof(*ctx_));
}

Aes256GcmSodiumEncrypter::~Aes256GcmSodiumEncrypter() { delete ctx_; }

bool Aes256GcmSodiumEncrypter::SetKey(const char *key, size_t key_len) {
  if (!AeadBaseEncrypter::SetKey(key, key_len)) {
    return false;
  }
  if (::crypto_aead_aes256gcm_beforenm(ctx_, key_) != 0) {
    return false;
  }
  return true;
}

bool Aes256GcmSodiumEncrypter::EncryptPacket(
    uint64_t packet_number, const char *associated_data,
    size_t associated_data_len, const char *plaintext, size_t plaintext_len,
    char *output, size_t *output_length, size_t max_output_length) {
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
  PacketNumberToNonce((uint8_t *)nonce_buffer, packet_number);

  if (::crypto_aead_aes256gcm_encrypt_afternm(
          reinterpret_cast<uint8_t *>(output), &ciphertext_size,
          reinterpret_cast<const uint8_t *>(plaintext), plaintext_len,
          reinterpret_cast<const uint8_t *>(associated_data),
          associated_data_len, nullptr,
          reinterpret_cast<const uint8_t *>(nonce_buffer), ctx_) != 0) {
    return false;
  }
  *output_length = ciphertext_size;
  return true;
}

uint32_t Aes256GcmSodiumEncrypter::cipher_id() const {
  return CRYPTO_AES256GCMSHA256;
}

} // namespace crypto

#endif // HAVE_LIBSODIUM
