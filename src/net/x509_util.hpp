// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_NET_X509_UTIL
#define H_NET_X509_UTIL

#include <string_view>
#include <openssl/base.h>
#include <openssl/pool.h>

namespace net::x509_util {

// Returns a CRYPTO_BUFFER_POOL for deduplicating certificates.
CRYPTO_BUFFER_POOL* GetBufferPool();

bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(std::string_view data);

} // namespace net::x509_util

#endif // H_NET_X509_UTIL
