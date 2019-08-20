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
  Channel() {}

  virtual ~Channel() {}

  virtual void connected() = 0;

  virtual void received(std::shared_ptr<IOBuf> buf) = 0;

  virtual void sent(std::shared_ptr<IOBuf> buf) = 0;

  virtual void disconnected(boost::system::error_code error) = 0;
};

#endif // H_CHANNEL
