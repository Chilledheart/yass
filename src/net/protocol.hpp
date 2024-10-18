// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_NET_PROTOCOL
#define H_NET_PROTOCOL

#ifdef _WIN32
#include <malloc.h>
#endif

#include <functional>
#include <string_view>
#include <utility>

#include <build/build_config.h>
#include "core/logging.hpp"
#include "net/asio.hpp"
#include "net/iobuf.hpp"

#define SOCKET_BUF_SIZE (16384)
#define SOCKET_DEBUF_SIZE (16384)
#define SS_FRAME_SIZE (16384 - 128)

namespace net {

// This enum is used in Net.SSLNegotiatedAlpnProtocol histogram.
// Do not change or re-use values.
enum NextProto { kProtoUnknown = 0, kProtoHTTP11 = 1, kProtoHTTP2 = 2, kProtoQUIC = 3, kProtoLast = kProtoQUIC };

// List of protocols to use for ALPN, used for configuring HttpNetworkSessions.
typedef std::vector<NextProto> NextProtoVector;

NextProto NextProtoFromString(std::string_view proto_string);

std::string_view NextProtoToString(NextProto next_proto);

#ifndef NDEBUG
inline void DumpHex_Impl(const char* file, int line, const char* prefix, const uint8_t* data, uint32_t length) {
  if (!VLOG_IS_ON(4)) {
    return;
  }
  char hex_buffer[4096];
  char* message = hex_buffer;
  int left_size = sizeof(hex_buffer) - 1;

  int written = snprintf(message, left_size, "%s LEN %u\n", prefix, length);
  if (written < 0 || written > left_size)
    goto done;
  message += written;
  left_size -= written;

  length = std::min<uint32_t>(length, sizeof(hex_buffer) / 4);
  for (uint32_t i = 0; i * 2 + 1 < length; ++i) {
    if (i % 8 == 0) {
      written = snprintf(message, left_size, "%s ", prefix);
      if (written < 0 || written > left_size)
        goto done;
      message += written;
      left_size -= written;
    }

    written = snprintf(message, left_size, "%02x%02x ", data[i * 2], data[i * 2 + 1]);
    if (written < 0 || written > left_size)
      goto done;
    message += written;
    left_size -= written;

    if ((i + 1) % 8 == 0) {
      written = snprintf(message, left_size, "\n");
      if (written < 0 || written > left_size)
        goto done;
      message += written;
      left_size -= written;
    }
  }
  written = snprintf(message, left_size, "\n");
  if (written < 0 || written > left_size)
    goto done;
  message += written;
  left_size -= written;

done:
  // ensure it is null-terminated
  hex_buffer[sizeof(hex_buffer) - 1] = '\0';
  ::yass::LogMessage(file, line, -4).stream() << hex_buffer;
}

inline void DumpHex_Impl(const char* file, int line, const char* prefix, const net::IOBuf* buf) {
  const uint8_t* data = buf->data();
  uint32_t length = buf->length();
  DumpHex_Impl(file, line, prefix, data, length);
}
#endif  // NDEBUG

}  // namespace net

#ifndef NDEBUG
#define DumpHex(...) ::net::DumpHex_Impl(__FILE__, __LINE__, __VA_ARGS__)
#else
#define DumpHex(...)
#endif

#endif  // H_NET_PROTOCOL
