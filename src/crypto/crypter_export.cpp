//
// crypter_export.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "crypto/crypter_export.hpp"

enum cipher_method cipher_method = PLAINTEXT;

enum cipher_method to_cipher_method(const std::string &method) {
  if (method == "chacha20-ietf-poly1305") {
    return CHACHA20POLY1305IETF;
  } else if (method == "xchacha20-ietf-poly1305") {
    return XCHACHA20POLY1305IETF;
#ifdef HAVE_BORINGSSL
  } else if (method == "chacha20-ietf-poly1305-evp") {
    return CHACHA20POLY1305IETF_EVP;
  } else if (method == "xchacha20-ietf-poly1305-evp") {
    return XCHACHA20POLY1305IETF_EVP;
  } else if (method == "aes128-gcm-evp") {
    return AES128GCMSHA256_EVP;
  } else if (method == "aes128-gcm12-evp") {
    return AES128GCM12SHA256_EVP;
  } else if (method == "aes192-gcm-evp") {
    return AES192GCMSHA256_EVP;
  } else if (method == "aes256-12-gcm-evp") {
    return AES256GCMSHA256_EVP;
#endif
  }
  return PLAINTEXT;
}

const char *to_cipher_method_str(enum cipher_method method) {
  switch (method) {
  case CHACHA20POLY1305IETF:
    return "chacha20-ietf-poly1305";
  case XCHACHA20POLY1305IETF:
    return "xchacha20-ietf-poly1305";
#ifdef HAVE_BORINGSSL
  case CHACHA20POLY1305IETF_EVP:
    return "chacha20-ietf-poly1305-evp";
  case XCHACHA20POLY1305IETF_EVP:
    return "xchacha20-ietf-poly1305-evp";
  case AES128GCMSHA256_EVP:
    return "aes128-gcm-evp";
  case AES128GCM12SHA256_EVP:
    return "aes128-gcm12-evp";
  case AES192GCMSHA256_EVP:
    return "aes192-gcm-evp";
  case AES256GCMSHA256_EVP:
    return "aes256-gcm-evp";
#endif
  default:
    return "plaintext";
  }
}

