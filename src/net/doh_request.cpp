// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/doh_request.hpp"

#include "net/dns_message_request.hpp"
#include "net/dns_message_response_parser.hpp"
#include "net/http_parser.hpp"

#ifdef _WIN32
#include <ws2tcpip.h>
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0x00000008
#endif
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace net {

// create addrinfo
static struct addrinfo* addrinfo_dup(bool is_ipv6, const net::dns_message::response& response, int port) {
  struct addrinfo* addrinfo = new struct addrinfo;
  addrinfo->ai_next = nullptr;
  // If hints.ai_flags includes the AI_CANONNAME flag, then the
  // ai_canonname field of the first of the addrinfo structures in the
  // returned list is set to point to the official name of the host.
  const char* canon_name = !response.cname().empty() ? response.cname().front().c_str() : nullptr;

  // Iterate the output
  struct addrinfo* next_addrinfo = addrinfo;
  if (is_ipv6) {
    for (const auto& aaaa : response.aaaa()) {
      next_addrinfo->ai_next = new struct addrinfo;
      next_addrinfo = next_addrinfo->ai_next;

      next_addrinfo->ai_flags = AI_CANONNAME | AI_NUMERICSERV;
      next_addrinfo->ai_family = AF_INET6;
      next_addrinfo->ai_socktype = SOCK_STREAM;
      next_addrinfo->ai_protocol = 0;
      next_addrinfo->ai_canonname = (char*)canon_name;

      struct sockaddr_in6* in6 = new struct sockaddr_in6;

      in6->sin6_family = AF_INET6;
      in6->sin6_port = htons(port);
      auto bytes = aaaa.to_bytes();
      memcpy(&in6->sin6_addr, &bytes, sizeof(bytes));

      next_addrinfo->ai_addr = (struct sockaddr*)in6;
      next_addrinfo->ai_addrlen = sizeof(*in6);
      next_addrinfo->ai_next = nullptr;

      canon_name = nullptr;
    }
  } else {
    for (const auto& a : response.a()) {
      next_addrinfo->ai_next = new struct addrinfo;
      next_addrinfo = next_addrinfo->ai_next;

      next_addrinfo->ai_flags = AI_CANONNAME | AI_NUMERICSERV;
      next_addrinfo->ai_family = AF_INET;
      next_addrinfo->ai_socktype = SOCK_STREAM;
      next_addrinfo->ai_protocol = 0;
      next_addrinfo->ai_canonname = (char*)canon_name;

      struct sockaddr_in* in = new struct sockaddr_in;

      in->sin_family = AF_INET;
      in->sin_port = htons(port);
      auto bytes = a.to_bytes();
      memcpy(&in->sin_addr, &bytes, sizeof(bytes));

      next_addrinfo->ai_addr = (struct sockaddr*)in;
      next_addrinfo->ai_addrlen = sizeof(*in);
      next_addrinfo->ai_next = nullptr;

      canon_name = nullptr;
    }
  }
  struct addrinfo* prev_addrinfo = addrinfo;
  addrinfo = addrinfo->ai_next;
  delete prev_addrinfo;
  return addrinfo;
}

// free addrinfo
void addrinfo_freedup(struct addrinfo* addrinfo) {
  struct addrinfo* next_addrinfo;
  while (addrinfo) {
    next_addrinfo = addrinfo->ai_next;
    if (addrinfo->ai_family == AF_INET6) {
      struct sockaddr_in6* in6 = (struct sockaddr_in6*)addrinfo->ai_addr;
      delete in6;
    } else {
      struct sockaddr_in* in = (struct sockaddr_in*)addrinfo->ai_addr;
      delete in;
    }
    delete addrinfo;
    addrinfo = next_addrinfo;
  }
}

using namespace dns_message;

void DoHRequest::close() {
  if (closed_) {
    return;
  }
  closed_ = true;
  cb_ = nullptr;
  if (ssl_socket_) {
    ssl_socket_->Disconnect();
  } else if (socket_.is_open()) {
    asio::error_code ec;
    socket_.close(ec);
  }
}

void DoHRequest::DoRequest(dns_message::DNStype dns_type, const std::string& host, int port, AsyncResolveCallback cb) {
  dns_type_ = dns_type;
  host_ = host;
  port_ = port;
  cb_ = std::move(cb);

  dns_message::request msg;
  if (!msg.init(host, dns_type)) {
    OnDoneRequest(asio::error::host_unreachable, nullptr);
    return;
  }
  buf_ = IOBuf::create(SOCKET_BUF_SIZE);
  for (auto buffer : msg.buffers()) {
    buf_->reserve(0, buffer.size());
    memcpy(buf_->mutable_tail(), buffer.data(), buffer.size());
    buf_->append(buffer.size());
  }

  asio::error_code ec;
  socket_.open(endpoint_.protocol(), ec);
  if (ec) {
    OnDoneRequest(ec, nullptr);
    return;
  }
  socket_.native_non_blocking(true, ec);
  socket_.non_blocking(true, ec);
  scoped_refptr<DoHRequest> self(this);
  socket_.async_connect(endpoint_, [this, self](asio::error_code ec) {
    // Cancelled, safe to ignore
    if (UNLIKELY(ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted)) {
      return;
    }
    if (ec) {
      OnDoneRequest(ec, nullptr);
      return;
    }
    VLOG(3) << "DoH Remote Server Connected: " << endpoint_;
    // tcp socket connected
    OnSocketConnect();
  });
}

void DoHRequest::OnSocketConnect() {
  scoped_refptr<DoHRequest> self(this);
  asio::error_code ec;
  SetTCPCongestion(socket_.native_handle(), ec);
  SetTCPKeepAlive(socket_.native_handle(), ec);
  SetSocketTcpNoDelay(&socket_, ec);
  ssl_socket_ = SSLSocket::Create(ssl_socket_data_index_, &io_context_, &socket_, ssl_ctx_,
                                  /*https_fallback*/ true, doh_host_);

  ssl_socket_->Connect([this, self](int rv) {
    asio::error_code ec;
    if (rv < 0) {
      ec = asio::error::connection_refused;
      OnDoneRequest(ec, nullptr);
      return;
    }
    VLOG(3) << "DoH Remote SSL Server Connected: " << endpoint_;
    // ssl socket connected
    OnSSLConnect();
  });
}

void DoHRequest::OnSSLConnect() {
  std::string request_header = absl::StrFormat(
      "POST %s HTTP/1.1\r\n"
      "Host: %s:%d\r\n"
      "Accept: */*\r\n"
      "Content-Type: application/dns-message\r\n"
      "Content-Length: %lu\r\n"
      "\r\n",
      doh_path_, doh_host_, doh_port_, buf_->length());
  buf_->reserve(request_header.size(), 0);
  memcpy(buf_->mutable_buffer(), request_header.c_str(), request_header.size());
  buf_->prepend(request_header.size());
  scoped_refptr<DoHRequest> self(this);

  recv_buf_ = IOBuf::create(SOCKET_BUF_SIZE);
  ssl_socket_->WaitWrite([this, self](asio::error_code ec) { OnSSLWritable(ec); });
  ssl_socket_->WaitRead([this, self](asio::error_code ec) { OnSSLReadable(ec); });
}

void DoHRequest::OnSSLWritable(asio::error_code ec) {
  if (ec) {
    OnDoneRequest(ec, nullptr);
    return;
  }
  size_t written = ssl_socket_->Write(buf_, ec);
  if (ec) {
    OnDoneRequest(ec, nullptr);
    return;
  }
  buf_->trimStart(written);
  if (LIKELY(buf_->empty())) {
    VLOG(3) << "DoH Request Sent: " << written;
    return;
  }
  scoped_refptr<DoHRequest> self(this);
  ssl_socket_->WaitWrite([this, self](asio::error_code ec) { OnSSLWritable(ec); });
}

void DoHRequest::OnSSLReadable(asio::error_code ec) {
  if (ec) {
    OnDoneRequest(ec, nullptr);
    return;
  }
  size_t read;
  do {
    ec = asio::error_code();
    read = ssl_socket_->Read(recv_buf_, ec);
    if (ec == asio::error::interrupted) {
      continue;
    }
  } while (false);

  if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
    OnDoneRequest(ec, nullptr);
    return;
  }
  recv_buf_->append(read);
  VLOG(3) << "DoH Response Received: " << read;

  OnReadResult();
}

void DoHRequest::OnReadResult() {
  HttpResponseParser parser;

  bool ok;
  int nparsed = parser.Parse(recv_buf_, &ok);
  if (nparsed) {
    VLOG(3) << "Connection (doh resolver) "
            << " http: " << std::string(reinterpret_cast<const char*>(recv_buf_->data()), nparsed);
  }

  if (ok && parser.status_code() == 200) {
    VLOG(3) << "DoH Response Parsed: Expected body: " << parser.content_length();
    recv_buf_->trimStart(nparsed);
    recv_buf_->retreat(nparsed);

    if (recv_buf_->length() < parser.content_length()) {
      OnDoneRequest(asio::error::connection_refused, {});
      return;
    }
    OnParseDnsResponse();
  } else {
    OnDoneRequest(asio::error::connection_refused, {});
    return;
  }
}

void DoHRequest::OnParseDnsResponse() {
  dns_message::response_parser response_parser;
  dns_message::response response;

  dns_message::response_parser::result_type result;
  std::tie(result, std::ignore) =
      response_parser.parse(response, recv_buf_->data(), recv_buf_->data(), recv_buf_->data() + recv_buf_->length());
  if (result != dns_message::response_parser::good) {
    OnDoneRequest(asio::error::connection_refused, {});
    return;
  }

  struct addrinfo* addrinfo = addrinfo_dup(dns_type_ == dns_message::DNS_TYPE_AAAA, response, port_);

  OnDoneRequest({}, addrinfo);
}

void DoHRequest::OnDoneRequest(asio::error_code ec, struct addrinfo* addrinfo) {
  if (auto cb = std::move(cb_)) {
    cb(ec, addrinfo);
  }
}

}  // namespace net
