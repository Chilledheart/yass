// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_NET_DOH_RESOLVER_HPP
#define H_NET_DOH_RESOLVER_HPP

#include <absl/functional/any_invocable.h>
#include <deque>
#include <string>
#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "net/asio.hpp"
#include "net/doh_request.hpp"

namespace net {

class DoHResolver : public RefCountedThreadSafe<DoHResolver> {
 public:
  DoHResolver(asio::io_context& io_context);
  static scoped_refptr<DoHResolver> Create(asio::io_context& io_context) {
    return MakeRefCounted<DoHResolver>(io_context);
  }
  ~DoHResolver();

  int Init(const std::string& doh_url, int timeout_ms);

  void SetupSSLContext(asio::error_code& ec);

  void Cancel();
  void Destroy();

  using AsyncResolveCallback = absl::AnyInvocable<void(asio::error_code ec, asio::ip::tcp::resolver::results_type)>;
  void AsyncResolve(const std::string& host, int port, AsyncResolveCallback cb);

 private:
  void DoRequest(bool enable_ipv6, const asio::ip::tcp::endpoint& endpoint);
  void OnDoneRequest(asio::error_code ec, asio::ip::tcp::resolver::results_type results);

  asio::io_context& io_context_;
  asio::ip::tcp::resolver resolver_;

  int ssl_socket_data_index_ = -1;
  bssl::UniquePtr<SSL_CTX> ssl_ctx_;

  bool init_ = false;
  std::string doh_url_;
  std::string doh_host_;
  int doh_port_;
  std::string doh_path_;
  int timeout_ms_ = 0;
  asio::steady_timer resolve_timer_;

  bool done_ = false;
  std::deque<asio::ip::tcp::endpoint> endpoints_;
  std::string host_;
  int port_;
  AsyncResolveCallback cb_;
  std::vector<scoped_refptr<DoHRequest>> reqs_;
};

}  // namespace net

#endif  // H_NET_DOH_RESOLVER_HPP
