// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_NET_X509_UTIL
#define H_NET_X509_UTIL

#include <openssl/base.h>
#include <openssl/pool.h>

namespace net::x509_util {

// Returns a CRYPTO_BUFFER_POOL for deduplicating certificates.
CRYPTO_BUFFER_POOL* GetBufferPool();

} // namespace net::x509_util

#endif // H_NET_X509_UTIL
