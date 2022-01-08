// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "crypto/aes_256_gcm_sodium_encrypter.hpp"

#ifdef HAVE_LIBSODIUM

#include <sodium/crypto_aead_aes256gcm.h>
#include <cstring>

#include "core/cipher.hpp"
#include "core/logging.hpp"

static const size_t kKeySize = crypto_aead_aes256gcm_KEYBYTES;
static const size_t kNonceSize = crypto_aead_aes256gcm_NPUBBYTES;

namespace crypto {

static_assert(Aes256GcmSodiumEncrypter::kAuthTagSize ==
                  crypto_aead_aes256gcm_ABYTES,
              "mismatched AES-256-GCM auth tag size");

Aes256GcmSodiumEncrypter::Aes256GcmSodiumEncrypter()
    : AeadBaseEncrypter(kKeySize, kAuthTagSize, kNonceSize),
      ctx_(new crypto_aead_aes256gcm_state) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
  memset(ctx_, 0, sizeof(*ctx_));
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

bool Aes256GcmSodiumEncrypter::EncryptPacket(uint64_t packet_number,
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

  if (::crypto_aead_aes256gcm_encrypt_afternm(
          reinterpret_cast<uint8_t*>(output), &ciphertext_size,
          reinterpret_cast<const uint8_t*>(plaintext), plaintext_len,
          reinterpret_cast<const uint8_t*>(associated_data),
          associated_data_len, nullptr, nonce, ctx_) != 0) {
    return false;
  }
  *output_length = ciphertext_size;
  return true;
}

uint32_t Aes256GcmSodiumEncrypter::cipher_id() const {
  return CRYPTO_AES256GCMSHA256;
}

}  // namespace crypto

#endif  // HAVE_LIBSODIUM
