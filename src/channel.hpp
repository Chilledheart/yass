//
// channel.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CHANNEL
#define H_CHANNEL

#include <boost/system/error_code.hpp>
#include <memory>

#include "iobuf.hpp"

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
  virtual void disconnected(boost::system::error_code error) = 0;
};

#endif // H_CHANNEL
