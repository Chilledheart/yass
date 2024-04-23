// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_NET_DOH_REQUEST_HPP
#define H_NET_DOH_REQUEST_HPP

#include <absl/strings/str_format.h>

#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "net/asio.hpp"
#include "net/dns_message.hpp"
#include "net/network.hpp"
#include "net/ssl_socket.hpp"

namespace net {

class DoHRequest : public RefCountedThreadSafe<DoHRequest> {
 public:
  DoHRequest(int ssl_socket_data_index,
             asio::io_context& io_context,
             asio::ip::tcp::endpoint endpoint,
             const std::string& doh_host,
             int doh_port,
             const std::string& doh_path,
             SSL_CTX* ssl_ctx)
      : io_context_(io_context),
        socket_(io_context),
        endpoint_(endpoint),
        doh_host_(doh_host),
        doh_port_(doh_port),
        doh_path_(doh_path),
        ssl_socket_(nullptr),
        ssl_socket_data_index_(ssl_socket_data_index),
        ssl_ctx_(ssl_ctx) {}

  template <typename... Args>
  static scoped_refptr<DoHRequest> Create(Args&&... args) {
    return MakeRefCounted<DoHRequest>(std::forward<Args>(args)...);
  }
  ~DoHRequest();

  void close();

  using AsyncResolveCallback = absl::AnyInvocable<void(asio::error_code ec, struct addrinfo* addrinfo)>;
  void DoRequest(dns_message::DNStype dns_type, const std::string& host, int port, AsyncResolveCallback cb);

 private:
  // tcp connect
  // ssl handshake
  // generate HTTP request
  // send HTTP request
  // receive HTTP response
  // parse HTTP body

  void OnSocketConnect();
  void OnSSLConnect();
  void OnSSLWritable(asio::error_code ec);
  void OnSSLReadable(asio::error_code ec);
  void OnReadHeader();
  void OnReadBody();
  void OnParseDnsResponse();
  void OnDoneRequest(asio::error_code ec, struct addrinfo* addrinfo);

 private:
  enum ReadState {
    Read_Header,
    Read_Body,
  };
  ReadState read_state_ = Read_Header;
  size_t body_length_ = 0;

 private:
  asio::io_context& io_context_;
  asio::ip::tcp::socket socket_;
  asio::ip::tcp::endpoint endpoint_;

  const std::string& doh_host_;
  const int doh_port_;
  const std::string& doh_path_;
  scoped_refptr<SSLSocket> ssl_socket_;
  const int ssl_socket_data_index_;
  SSL_CTX* ssl_ctx_;

  bool closed_ = false;
  dns_message::DNStype dns_type_;
  std::string host_;
  int port_;
  std::string service_;
  AsyncResolveCallback cb_;
  std::shared_ptr<IOBuf> buf_;
  std::shared_ptr<IOBuf> recv_buf_;
};

}  // namespace net

#endif  // H_NET_DOH_REQUEST_HPP
