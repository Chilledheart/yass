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

using boost::asio::ip::tcp;
using namespace socks5;

static tcp::endpoint resolveEndpoint(boost::asio::io_context &io_context,
                                     const std::string &host, uint16_t port) {
  boost::system::error_code ec;
  boost::asio::ip::tcp::resolver resolver(io_context);
  auto endpoints = resolver.resolve(host, std::to_string(port), ec);
  return endpoints->endpoint();
}

Worker::Worker()
    : work_guard_(boost::asio::make_work_guard(io_context_)),
      thread_(std::bind(&Worker::WorkFunc, this)) {}

Worker::~Worker() {
  Stop();
  work_guard_.reset();
  thread_.join();
}

void Worker::Start() {
  endpoint_ = resolveEndpoint(io_context_, FLAGS_local_host, FLAGS_local_port);
  remote_endpoint_ =
      resolveEndpoint(io_context_, FLAGS_server_host, FLAGS_server_port);

  /// listen in the worker thread
  io_context_.post([this]() {
    socks5_server_ =
        std::make_unique<Socks5Factory>(io_context_, remote_endpoint_);

    bool successed = false;
    std::string msg;

    try {
      socks5_server_->listen(endpoint_);
      successed = true;
    } catch (std::exception &e) {
      msg = e.what();
    }

    wxCommandEvent *evt =
        new wxCommandEvent(MY_EVENT, successed ? ID_STARTED : ID_START_FAILED);
    evt->SetString(msg.c_str());
    wxTheApp->QueueEvent(evt);
  });
}

void Worker::Stop() {
  /// stop in the worker thread
  io_context_.post([this]() {
    if (socks5_server_) {
      socks5_server_->stop();
      wxCommandEvent *evt = new wxCommandEvent(MY_EVENT, ID_STOPPED);
      wxTheApp->QueueEvent(evt);
    }
  });
}

void Worker::WorkFunc() { io_context_.run(); }
