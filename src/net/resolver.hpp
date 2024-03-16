// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_NET_RESOLVER_HPP
#define H_NET_RESOLVER_HPP

#include "core/utils.hpp"
#include "net/asio.hpp"
#include "net/doh_resolver.hpp"

#ifdef HAVE_C_ARES
#include "net/c-ares.hpp"
#endif

namespace net {

class Resolver {
 public:
  Resolver(asio::io_context& io_context);

  int Init();

  void Cancel() {
    if (!doh_url_.empty()) {
      if (doh_resolver_) {
        doh_resolver_->Cancel();
      }
      return;
    }
#ifdef HAVE_C_ARES
    if (resolver_) {
      resolver_->Cancel();
    }
#else
    resolver_.cancel();
#endif
  }

  void Reset() {
    if (!doh_url_.empty()) {
      doh_resolver_.reset();
      return;
    }
#ifdef HAVE_C_ARES
    resolver_.reset();
#endif
  }

  using AsyncResolveCallback = std::function<void(asio::error_code ec, asio::ip::tcp::resolver::results_type)>;
  void AsyncResolve(const std::string& host_name, int port, AsyncResolveCallback cb) {
    if (!doh_url_.empty()) {
      doh_resolver_->AsyncResolve(host_name, port, cb);
      return;
    }
#ifdef HAVE_C_ARES
    resolver_->AsyncResolve(host_name, std::to_string(port), cb);
#else
    resolver_.async_resolve(Net_ipv6works() ? asio::ip::tcp::unspec() : asio::ip::tcp::v4(), host_name,
                            std::to_string(port), cb);
#endif
  }

 private:
  asio::io_context& io_context_;
  std::string doh_url_;
  scoped_refptr<DoHResolver> doh_resolver_;

#ifdef HAVE_C_ARES
  scoped_refptr<CAresResolver> resolver_;
#else
  asio::ip::tcp::resolver resolver_;
#endif
};

}  // namespace net

#endif  // H_NET_RESOLVER_HPP
