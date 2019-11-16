//
// aes_256_gcm_sodium_decrypter.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "crypto/aes_256_gcm_sodium_decrypter.hpp"

#include "core/cipher.hpp"
#include <glog/logging.h>
#include <sodium/crypto_aead_aes256gcm.h>
#include <sodium/utils.h>

static const size_t kKeySize = crypto_aead_aes256gcm_KEYBYTES;
static const size_t kNonceSize = crypto_aead_aes256gcm_NPUBBYTES;

namespace crypto {

static_assert(Aes256GcmSodiumDecrypter::kAuthTagSize == crypto_aead_aes256gcm_ABYTES,
    "mismatched AES-256-GCM auth tag size");

Aes256GcmSodiumDecrypter::Aes256GcmSodiumDecrypter()
    : AeadBaseDecrypter(kKeySize, kAuthTagSize, kNonceSize),
      ctx_(new crypto_aead_aes256gcm_state) {
  ::sodium_memzero(ctx_, sizeof(*ctx_));
}

Aes256GcmSodiumDecrypter::~Aes256GcmSodiumDecrypter() {
  delete ctx_;
}

bool Aes256GcmSodiumDecrypter::SetKey(const char* key, size_t key_len) {
  if (!AeadBaseDecrypter::SetKey(key, key_len)) {
    return false;
  }
  if (::crypto_aead_aes256gcm_beforenm(ctx_, key_) != 0) {
    return false;
  }
  return true;
}

bool Aes256GcmSodiumDecrypter::DecryptPacket(
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
#if 0
  size_t prefix_len = nonce_size_ - sizeof(packet_number);
  memcpy(nonce + prefix_len, &packet_number, sizeof(packet_number));
#endif

  if (::crypto_aead_aes256gcm_decrypt_afternm(
          reinterpret_cast<uint8_t *>(output), &plaintext_size, nullptr,
          reinterpret_cast<const uint8_t *>(ciphertext), ciphertext_len,
          reinterpret_cast<const uint8_t *>(associated_data), associated_data_len,
          reinterpret_cast<const uint8_t *>(nonce),
          ctx_) != 0) {
    return false;
  }
  *output_length = plaintext_size;
  return true;
}

uint32_t Aes256GcmSodiumDecrypter::cipher_id() const {
  return CRYPTO_AES256GCMSHA256;
}

} // namespace crypto
