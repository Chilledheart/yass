// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/x509_util.hpp"

namespace net::x509_util {

namespace {

class BufferPoolSingleton {
 public:
  BufferPoolSingleton() : pool_(CRYPTO_BUFFER_POOL_new()) {}
  CRYPTO_BUFFER_POOL* pool() { return pool_; }

 private:
  // The singleton is leaky, so there is no need to use a smart pointer.
  CRYPTO_BUFFER_POOL* pool_;
};

}  // namespace

CRYPTO_BUFFER_POOL* GetBufferPool() {
  static BufferPoolSingleton g_buffer_pool_singleton;
  return g_buffer_pool_singleton.pool();
}

bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(std::string_view data) {
  return bssl::UniquePtr<CRYPTO_BUFFER>(
      CRYPTO_BUFFER_new(reinterpret_cast<const uint8_t*>(data.data()), data.size(), GetBufferPool()));
}

}  // namespace net::x509_util
