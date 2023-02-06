// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_CHANNEL
#define H_CHANNEL

#include <memory>

#include "core/asio.hpp"
#include "core/iobuf.hpp"

class Channel {
 public:
  /// Construct the channel
  Channel() = default;

  /// Deconstruct the channel
  virtual ~Channel() = default;

  /// Called once connection established
  virtual void connected() = 0;

  /// Called once ready to receive
  ///
  virtual void received() = 0;

  /// Called once ready to write
  ///
  virtual void sent() = 0;

  /// Called once connection closed
  virtual void disconnected(asio::error_code error) = 0;
};

#endif  // H_CHANNEL
