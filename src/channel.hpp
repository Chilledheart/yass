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

  /// Called if stream's certificate is required
  virtual std::string retrive_certificate() { return {}; }

  /// Called once connection established
  virtual void connected() = 0;

  /// Called once an IOBuf is received
  ///
  /// \param buf the io buf received
  virtual void received(std::shared_ptr<IOBuf> buf) = 0;

  /// Called once an IOBuf is sent
  ///
  /// \param buf the io buf sent
  /// \param bytes_transferred the bytes sent
  virtual void sent(std::shared_ptr<IOBuf> buf, size_t bytes_transferred) = 0;

  /// Called once connection closed
  virtual void disconnected(asio::error_code error) = 0;
};

#endif  // H_CHANNEL
