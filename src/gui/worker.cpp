// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "worker.hpp"

#include "yass.hpp"

using asio::ip::tcp;
using namespace socks5;

static tcp::endpoint resolveEndpoint(asio::io_context &io_context,
                                     const std::string &host, uint16_t port,
                                     asio::error_code &ec) {
  asio::ip::tcp::resolver resolver(io_context);
  auto endpoints = resolver.resolve(host, std::to_string(port), ec);
  if (ec) {
    LOG(WARNING) << "name resolved failed due to: " << ec;
    return tcp::endpoint();
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
  asio::error_code ec_old;
  endpoint_ =
      resolveEndpoint(io_context_, FLAGS_local_host, FLAGS_local_port, ec_old);
  if (!ec_old) {
    remote_endpoint_ = resolveEndpoint(io_context_, FLAGS_server_host,
                                       FLAGS_server_port, ec_old);
  }

  /// listen in the worker thread
  io_context_.post([this, quiet, ec_old]() {
    socks5_server_ =
        std::make_unique<Socks5Factory>(io_context_, remote_endpoint_);

    bool successed = false;
    std::string msg;
    asio::error_code ec = ec_old;

    if (!ec) {
      socks5_server_->listen(endpoint_, SOMAXCONN, ec);
    }

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

    if (!wxTheApp) {
      return;
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
    }
    if (wxTheApp) {
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
