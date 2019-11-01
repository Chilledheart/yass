//
// worker.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "cipher.hpp"
#include "config.hpp"
#include "socks5_factory.hpp"

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <memory>

class Worker {
public:
  Worker();
  ~Worker();

  void Start();
  void Stop();

  const boost::asio::ip::tcp::endpoint &GetEndpoint() const {
    return endpoint_;
  }

  const boost::asio::ip::tcp::endpoint &GetRemoteEndpoint() const {
    return remote_endpoint_;
  }

  size_t currentConnections() const {
    return socks5_server_ ? socks5_server_->currentConnections() : 0;
  }

private:
  void WorkFunc();

  boost::asio::io_context io_context_;
  /// stopping the io_context from running out of work
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      work_guard_;

  std::unique_ptr<Socks5Factory> socks5_server_;
  boost::asio::ip::tcp::endpoint endpoint_;
  boost::asio::ip::tcp::endpoint remote_endpoint_;

  boost::thread thread_;
};
