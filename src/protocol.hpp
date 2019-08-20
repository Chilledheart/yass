//
// protocol.hpp
// ~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_PROTOCOL
#define H_PROTOCOL

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <functional>
#include <glog/logging.h>
#include <utility>

#include "iobuf.hpp"

#define SOCKET_BUF_SIZE (4 * 1024)

inline void DumpHex(const char *prefix, const uint8_t *data, uint32_t length) {
  if (!VLOG_IS_ON(4)) {
    return;
  }
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
  uint64_t length = buf->length();
  DumpHex(prefix, data, length);
}

#endif // H_PROTOCOL
