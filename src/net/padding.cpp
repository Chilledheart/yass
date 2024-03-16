// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/padding.hpp"

#include "core/rand_util.hpp"

/// <payload_length> <padding length> <payload> <padding>
/// input:
///                                       *
/// output:
/// p[0] << 8 + p[1]       p[2]           *         *
ABSL_FLAG(bool, padding_support, true, "Enable padding support");

namespace net {

void AddPadding(std::shared_ptr<net::IOBuf> buf) {
  size_t payload_size = buf->length();
  DCHECK_LE(payload_size, 0xffffu);
  size_t padding_size = RandInt(0, kMaxPaddingSize);
  buf->reserve(kPaddingHeaderSize, padding_size);

  uint8_t* p = buf->mutable_buffer();
  p[0] = payload_size >> 8;
  p[1] = payload_size & 0xff;
  p[2] = padding_size;
  memset(buf->mutable_tail(), 0, padding_size);

  buf->prepend(kPaddingHeaderSize);
  buf->append(padding_size);
}

/// <payload_length> <padding length> <payload> <padding>
/// input:
/// p[0] << 8 + p[1]       p[2]           *         *
/// output:
///                                       *
std::shared_ptr<net::IOBuf> RemovePadding(std::shared_ptr<net::IOBuf> buf, asio::error_code& ec) {
  if (buf->length() < kPaddingHeaderSize) {
    ec = asio::error::try_again;
    return nullptr;
  }
  const uint8_t* p = buf->data();
  size_t payload_size = (p[0] << 8) + p[1];
  size_t padding_size = p[2];
  if (buf->length() < kPaddingHeaderSize + payload_size + padding_size) {
    ec = asio::error::try_again;
    return nullptr;
  }
  buf->trimStart(kPaddingHeaderSize);
  std::shared_ptr<net::IOBuf> result = net::IOBuf::copyBuffer(buf->data(), payload_size);
  buf->trimStart(payload_size + padding_size);
  buf->retreat(kPaddingHeaderSize + payload_size + padding_size);
  ec = asio::error_code();
  return result;
}

}  // namespace net
