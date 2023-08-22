// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#include "crypto/aead_mbedtls_decrypter.hpp"

#include "core/logging.hpp"
#include "protocol.hpp"

#ifdef HAVE_MBEDTLS

#include "crypto/mbedtls_common.hpp"

namespace crypto {

AeadMbedtlsDecrypter::AeadMbedtlsDecrypter(cipher_method method,
                                           mbedtls_cipher_context_t* evp,
                                           size_t key_size,
                                           size_t auth_tag_size,
                                           size_t nonce_size)
    : AeadBaseDecrypter(key_size, auth_tag_size, nonce_size),
      method_(method), evp_(evp) {
#if 0
  DCHECK_EQ(EVP_AEAD_key_length(aead_alg_), key_size);
  DCHECK_EQ(EVP_AEAD_nonce_length(aead_alg_), nonce_size);
  DCHECK_GE(EVP_AEAD_max_tag_len(aead_alg_), auth_tag_size);
#endif
}

AeadMbedtlsDecrypter::~AeadMbedtlsDecrypter() {
  mbedtls_release_evp(evp_);
}

bool AeadMbedtlsDecrypter::SetKey(const char* key, size_t key_len) {
  if (!AeadBaseDecrypter::SetKey(key, key_len)) {
    return false;
  }
  if (mbedtls_cipher_setkey(evp_, (const uint8_t*)key, key_len * 8, MBEDTLS_DECRYPT) != 0) {
    return false;
  }
  if (mbedtls_cipher_set_iv(evp_, iv_, nonce_size_) != 0) {
    return false;
  }
  if (mbedtls_cipher_reset(evp_) != 0) {
    return false;
  }
  return true;
}

bool AeadMbedtlsDecrypter::DecryptPacket(uint64_t packet_number,
                                        const char* associated_data,
                                        size_t associated_data_len,
                                        const char* ciphertext,
                                        size_t ciphertext_len,
                                        char* output,
                                        size_t* output_length,
                                        size_t max_output_length) {
  if (ciphertext_len < auth_tag_size_) {
    return false;
  }

  if (have_preliminary_key_) {
    LOG(ERROR) << "Unable to decrypt while key diversification is pending";
    return false;
  }

  uint8_t nonce[kMaxNonceSize] = {};
  memcpy(nonce, iv_, nonce_size_);

  // for libsodium, packet number is written ahead
  PacketNumberToNonceSodium(nonce, nonce_size_, packet_number);

  DumpHex("DE-NONCE", nonce, nonce_size_);

  if (mbedtls_cipher_update(evp_,
                            reinterpret_cast<const uint8_t*>(ciphertext),
                            ciphertext_len,
                            reinterpret_cast<uint8_t*>(output), output_length) != 0) {
    return false;
  }
  return true;
}

}  // namespace crypto

#endif  // HAVE_MBEDTLS
