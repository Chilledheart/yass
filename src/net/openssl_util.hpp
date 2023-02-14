// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_NET_OPENSSL_UTIL
#define H_NET_OPENSSL_UTIL

#include <cstdint>

namespace net {
int OpenSSLNetErrorLib();
int MapOpenSSLErrorSSL(uint32_t error_code);
int MapOpenSSLErrorWithDetails(int err);
void OpenSSLPutNetError(const char* file, int line, int err);

inline int MapOpenSSLError(int err) {
  return MapOpenSSLErrorWithDetails(err);
}

} // namespace net

#define FROM_HERE __FILE__, __LINE__

#endif // H_NET_OPENSSL_UTIL
