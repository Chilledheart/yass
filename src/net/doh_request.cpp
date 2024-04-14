// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/doh_request.hpp"

#include "net/dns_addrinfo_helper.hpp"
#include "net/dns_message_request.hpp"
#include "net/dns_message_response_parser.hpp"
#include "net/http_parser.hpp"

namespace net {

using namespace dns_message;

DoHRequest::~DoHRequest() {
  VLOG(1) << "DoH Request freed memory";

  close();
}

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

  if (is_localhost(host_)) {
    VLOG(3) << "DoH Request: is_localhost host: " << host_;
    scoped_refptr<DoHRequest> self(this);
    asio::post(io_context_, [this, self]() {
      struct addrinfo* addrinfo = addrinfo_loopback(dns_type_ == dns_message::DNS_TYPE_AAAA, port_);
      OnDoneRequest({}, addrinfo);
    });
    return;
  }

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

  {
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
  scoped_refptr<DoHRequest> self(this);

  recv_buf_ = IOBuf::create(UINT16_MAX);
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
  VLOG(3) << "DoH Request Sent: " << written << " bytes Remaining: " << buf_->length() << " bytes";
  if (UNLIKELY(!buf_->empty())) {
    scoped_refptr<DoHRequest> self(this);
    ssl_socket_->WaitWrite([this, self](asio::error_code ec) { OnSSLWritable(ec); });
    return;
  }
  VLOG(3) << "DoH Request Fully Sent";
}

void DoHRequest::OnSSLReadable(asio::error_code ec) {
  if (UNLIKELY(ec)) {
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

  if (UNLIKELY(ec && ec != asio::error::try_again && ec != asio::error::would_block)) {
    OnDoneRequest(ec, nullptr);
    return;
  }
  recv_buf_->append(read);

  VLOG(3) << "DoH Response Received: " << read << " bytes";

  switch (read_state_) {
    case Read_Header:
      OnReadHeader();
      break;
    case Read_Body:
      OnReadBody();
      break;
  }
}

void DoHRequest::OnReadHeader() {
  DCHECK_EQ(read_state_, Read_Header);
  HttpResponseParser parser;

  bool ok;
  int nparsed = parser.Parse(recv_buf_, &ok);
  if (nparsed) {
    VLOG(3) << "Connection (doh resolver) "
            << " http: " << std::string(reinterpret_cast<const char*>(recv_buf_->data()), nparsed);
  }
  if (!ok) {
    LOG(WARNING) << "DoH Response Invalid HTTP Response";
    OnDoneRequest(asio::error::operation_not_supported, nullptr);
    return;
  }

  VLOG(3) << "DoH Response Header Parsed: " << nparsed << " bytes";
  recv_buf_->trimStart(nparsed);
  recv_buf_->retreat(nparsed);

  if (UNLIKELY(parser.status_code() != 200)) {
    LOG(WARNING) << "DoH Response Unexpected HTTP Response Status Code: " << parser.status_code();
    OnDoneRequest(asio::error::operation_not_supported, nullptr);
    return;
  }

  if (UNLIKELY(parser.content_type() != "application/dns-message")) {
    LOG(WARNING) << "DoH Response Expected Type: application/dns-message but received: " << parser.content_type();
    OnDoneRequest(asio::error::operation_not_supported, nullptr);
    return;
  }

  if (UNLIKELY(parser.content_length() == 0)) {
    LOG(WARNING) << "DoH Response Missing Content Length";
    OnDoneRequest(asio::error::operation_not_supported, nullptr);
    return;
  }

  if (UNLIKELY(parser.content_length() >= UINT16_MAX)) {
    LOG(WARNING) << "DoH Response Too Large: " << parser.content_length() << " bytes";
    OnDoneRequest(asio::error::operation_not_supported, nullptr);
    return;
  }

  read_state_ = Read_Body;
  body_length_ = parser.content_length();

  OnReadBody();
}

void DoHRequest::OnReadBody() {
  DCHECK_EQ(read_state_, Read_Body);
  if (UNLIKELY(recv_buf_->length() < body_length_)) {
    VLOG(3) << "DoH Response Expected Data: " << body_length_ << " bytes Current: " << recv_buf_->length() << " bytes";

    scoped_refptr<DoHRequest> self(this);
    recv_buf_->reserve(0, body_length_ - recv_buf_->length());
    ssl_socket_->WaitRead([this, self](asio::error_code ec) { OnSSLReadable(ec); });
    return;
  }

  OnParseDnsResponse();
}

void DoHRequest::OnParseDnsResponse() {
  DCHECK_EQ(read_state_, Read_Body);
  DCHECK_GE(recv_buf_->length(), body_length_);

  dns_message::response_parser response_parser;
  dns_message::response response;

  dns_message::response_parser::result_type result;
  std::tie(result, std::ignore) =
      response_parser.parse(response, recv_buf_->data(), recv_buf_->data(), recv_buf_->data() + body_length_);
  if (result != dns_message::response_parser::good) {
    LOG(WARNING) << "DoH Response Bad Format";
    OnDoneRequest(asio::error::operation_not_supported, {});
    return;
  }
  VLOG(3) << "DoH Response Body Parsed: " << body_length_ << " bytes";
  recv_buf_->trimStart(body_length_);
  recv_buf_->retreat(body_length_);

  struct addrinfo* addrinfo = addrinfo_dup(dns_type_ == dns_message::DNS_TYPE_AAAA, response, port_);

  OnDoneRequest({}, addrinfo);
}

void DoHRequest::OnDoneRequest(asio::error_code ec, struct addrinfo* addrinfo) {
  if (auto cb = std::move(cb_)) {
    cb(ec, addrinfo);
  } else {
    addrinfo_freedup(addrinfo);
  }
}

}  // namespace net
