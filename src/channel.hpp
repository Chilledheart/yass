// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CHANNEL
#define H_CHANNEL

#include <memory>

#include "core/iobuf.hpp"
#include <asio/error_code.hpp>

class Channel {
public:
  /// Construct the channel
  Channel() {}

  /// Deconstruct the channel
  virtual ~Channel() {}

  /// Called once connection established
  virtual void connected() = 0;

  /// Called once an IOBuf is received
  ///
  /// \param buf the io buf received
  virtual void received(std::shared_ptr<IOBuf> buf) = 0;

  /// Called once an IOBuf is sent
  ///
  /// \param buf the io buf sent
  virtual void sent(std::shared_ptr<IOBuf> buf) = 0;

  /// Called once connection closed
  virtual void disconnected(asio::error_code error) = 0;
};

#endif // H_CHANNEL
