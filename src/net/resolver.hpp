// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_NET_RESOLVER_HPP
#define H_NET_RESOLVER_HPP

#include <functional>

#include "net/asio.hpp"

namespace net {

class Resolver {
  class ResolverImpl;

 public:
  Resolver(asio::io_context& io_context);
  ~Resolver();

  int Init();
  void Cancel();
  void Reset();

  using AsyncResolveCallback = std::function<void(asio::error_code ec, asio::ip::tcp::resolver::results_type)>;
  void AsyncResolve(const std::string& host_name, int port, AsyncResolveCallback cb);

 private:
  ResolverImpl* impl_ = nullptr;
};

}  // namespace net

#endif  // H_NET_RESOLVER_HPP
