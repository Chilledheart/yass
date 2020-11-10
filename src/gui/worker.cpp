//
// worker.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "worker.hpp"

#include "yass.hpp"

using asio::ip::tcp;
using namespace socks5;

static tcp::endpoint resolveEndpoint(asio::io_context &io_context,
                                     const std::string &host, uint16_t port) {
  asio::error_code ec;
  asio::ip::tcp::resolver resolver(io_context);
  auto endpoints = resolver.resolve(host, std::to_string(port), ec);
  if (ec) {
    LOG(WARNING) << "name resolved failed due to: " << ec;
    // TODO
  }
  return endpoints->endpoint();
}

Worker::Worker()
    : work_guard_(asio::make_work_guard(io_context_)),
      thread_(std::bind(&Worker::WorkFunc, this)) {}

Worker::~Worker() {
  Stop(true);
  work_guard_.reset();
  thread_.join();
}

void Worker::Start(bool quiet) {
  endpoint_ = resolveEndpoint(io_context_, FLAGS_local_host, FLAGS_local_port);
  remote_endpoint_ =
      resolveEndpoint(io_context_, FLAGS_server_host, FLAGS_server_port);

  /// listen in the worker thread
  io_context_.post([this, quiet]() {
    socks5_server_ =
        std::make_unique<Socks5Factory>(io_context_, remote_endpoint_);

    bool successed = false;
    std::string msg;

    asio::error_code ec = socks5_server_->listen(endpoint_);

    if (quiet) {
      return;
    }

    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      msg = ec.message();
      successed = false;
    } else {
      successed = true;
    }

    wxCommandEvent *evt =
        new wxCommandEvent(MY_EVENT, successed ? ID_STARTED : ID_START_FAILED);
    evt->SetString(msg.c_str());
    wxTheApp->QueueEvent(evt);
  });
}

void Worker::Stop(bool quiet) {
  /// stop in the worker thread
  io_context_.post([this, quiet]() {
    if (socks5_server_) {
      socks5_server_->stop();
      if (quiet) {
        return;
      }
      wxCommandEvent *evt = new wxCommandEvent(MY_EVENT, ID_STOPPED);
      wxTheApp->QueueEvent(evt);
    }
  });
}

void Worker::WorkFunc() {
  asio::error_code ec = asio::error_code();
  io_context_.run(ec);

  if (ec) {
    LOG(ERROR) << "io_context failed due to: " << ec;
  }
}
