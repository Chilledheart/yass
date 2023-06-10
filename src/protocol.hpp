// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_PROTOCOL
#define H_PROTOCOL

#ifdef _WIN32
#include <malloc.h>
#endif

#include <functional>
#include <utility>

#include "core/asio.hpp"
#include "core/iobuf.hpp"
#include "core/logging.hpp"

#define SOCKET_BUF_SIZE (64*1024-128)
#define SOCKET_DEBUF_SIZE (64*1024-8)
#define SS_FRAME_SIZE (16384-128)

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

    written =
        snprintf(message, left_size, "%02x%02x ", data[i * 2], data[i * 2 + 1]);
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
  LogMessage(file, line, -4).stream()
    << hex_buffer;
}

inline void DumpHex_Impl(const char* file, int line, const char* prefix, const IOBuf* buf) {
  const uint8_t* data = buf->data();
  uint32_t length = buf->length();
  DumpHex_Impl(file, line, prefix, data, length);
}
#define DumpHex(...) DumpHex_Impl(__FILE__, __LINE__, __VA_ARGS__)
#else
#define DumpHex(...)
#endif

#endif  // H_PROTOCOL
