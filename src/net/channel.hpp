// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_NET_CHANNEL
#define H_NET_CHANNEL

#include <memory>

#include "net/asio.hpp"
#include "net/iobuf.hpp"

namespace net {

class Channel {
 public:
  /// Construct the channel
  Channel() = default;

  /// Deconstruct the channel
  virtual ~Channel() = default;

  /// Called once connection closed
  virtual void disconnected(asio::error_code error) = 0;
};

}  // namespace net

#endif  // H_NET_CHANNEL
