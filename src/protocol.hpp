// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_PROTOCOL
#define H_PROTOCOL

#ifdef _WIN32
#include <malloc.h>
#endif

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <functional>
#include <utility>

#include "core/iobuf.hpp"
#include "core/logging.hpp"

#define SOCKET_BUF_SIZE (4096)

#ifndef NDEBUG
inline void DumpHex(const char *prefix, const uint8_t *data, uint32_t length) {
  if (!VLOG_IS_ON(3)) {
    return;
  }
  fprintf(stderr, "%s LEN %u\n", prefix, length);
  length = std::min(length, 32U);
  for (uint32_t i = 0; i * 2 + 1 < length; ++i) {
    if (i % 8 == 0) {
      fprintf(stderr, "%s ", prefix);
    }
    fprintf(stderr, "%02x%02x ", data[i * 2], data[i * 2 + 1]);
    if ((i + 1) % 8 == 0) {
      fprintf(stderr, "\n");
    }
  }
  fprintf(stderr, "\n");
}

inline void DumpHex(const char *prefix, const IOBuf *buf) {
  const uint8_t *data = buf->data();
  uint32_t length = buf->length();
  DumpHex(prefix, data, length);
}
#else
#define DumpHex(...)
#endif

#endif // H_PROTOCOL
