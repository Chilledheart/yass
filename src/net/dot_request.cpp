// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/dot_request.hpp"

#include "net/dns_addrinfo_helper.hpp"
#include "net/dns_message_request.hpp"
#include "net/dns_message_response_parser.hpp"

namespace net {

using namespace dns_message;

DoTRequest::~DoTRequest() {
  VLOG(1) << "DoT Request freed memory";

  close();
}

void DoTRequest::close() {
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

void DoTRequest::DoRequest(dns_message::DNStype dns_type, const std::string& host, int port, AsyncResolveCallback cb) {
  dns_type_ = dns_type;
  host_ = host;
  port_ = port;
  cb_ = std::move(cb);

  if (is_localhost(host_)) {
    VLOG(3) << "DoT Request: is_localhost host: " << host_;
    scoped_refptr<DoTRequest> self(this);
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
    uint16_t length = htons(buf_->length());
    buf_->reserve(sizeof(length), 0);
    memcpy(buf_->mutable_buffer(), &length, sizeof(length));
    buf_->prepend(sizeof(length));
  }

  asio::error_code ec;
  socket_.open(endpoint_.protocol(), ec);
  if (ec) {
    OnDoneRequest(ec, nullptr);
    return;
  }
  socket_.native_non_blocking(true, ec);
  socket_.non_blocking(true, ec);
  scoped_refptr<DoTRequest> self(this);
  socket_.async_connect(endpoint_, [this, self](asio::error_code ec) {
    // Cancelled, safe to ignore
    if (UNLIKELY(ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted)) {
      return;
    }
    if (ec) {
      OnDoneRequest(ec, nullptr);
      return;
    }
    VLOG(3) << "DoT Remote Server Connected: " << endpoint_;
    // tcp socket connected
    OnSocketConnect();
  });
}

void DoTRequest::OnSocketConnect() {
  scoped_refptr<DoTRequest> self(this);
  asio::error_code ec;
  SetTCPCongestion(socket_.native_handle(), ec);
  SetTCPKeepAlive(socket_.native_handle(), ec);
  SetSocketTcpNoDelay(&socket_, ec);
  ssl_socket_ = SSLSocket::Create(ssl_socket_data_index_, &io_context_, &socket_, ssl_ctx_,
                                  /*https_fallback*/ true, dot_host_);

  ssl_socket_->Connect([this, self](int rv) {
    asio::error_code ec;
    if (rv < 0) {
      ec = asio::error::connection_refused;
      OnDoneRequest(ec, nullptr);
      return;
    }
    VLOG(3) << "DoT Remote SSL Server Connected: " << endpoint_;
    // ssl socket connected
    OnSSLConnect();
  });
}

void DoTRequest::OnSSLConnect() {
  scoped_refptr<DoTRequest> self(this);

  recv_buf_ = IOBuf::create(sizeof(uint16_t) + UINT16_MAX);
  ssl_socket_->WaitWrite([this, self](asio::error_code ec) { OnSSLWritable(ec); });
  ssl_socket_->WaitRead([this, self](asio::error_code ec) { OnSSLReadable(ec); });
}

void DoTRequest::OnSSLWritable(asio::error_code ec) {
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
  VLOG(3) << "DoT Request Sent: " << written << " bytes Remaining: " << buf_->length() << " bytes";
  if (UNLIKELY(!buf_->empty())) {
    scoped_refptr<DoTRequest> self(this);
    ssl_socket_->WaitWrite([this, self](asio::error_code ec) { OnSSLWritable(ec); });
    return;
  }
  VLOG(3) << "DoT Request Fully Sent";
}

void DoTRequest::OnSSLReadable(asio::error_code ec) {
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

  VLOG(3) << "DoT Response Received: " << read << " bytes";

  switch (read_state_) {
    case Read_Header:
      OnReadHeader();
      break;
    case Read_Body:
      OnReadBody();
      break;
  }
}

void DoTRequest::OnReadHeader() {
  DCHECK_EQ(read_state_, Read_Header);
  uint16_t length;

  if (recv_buf_->length() < sizeof(length)) {
    LOG(WARNING) << "DoT Response Invalid HTTP Response";
    OnDoneRequest(asio::error::operation_not_supported, nullptr);
    return;
  }

  memcpy(&length, recv_buf_->data(), sizeof(length));

  VLOG(3) << "DoT Response Header Parsed: " << sizeof(length) << " bytes";
  recv_buf_->trimStart(sizeof(length));
  recv_buf_->retreat(sizeof(length));

  read_state_ = Read_Body;
  body_length_ = ntohs(length);

  OnReadBody();
}

void DoTRequest::OnReadBody() {
  DCHECK_EQ(read_state_, Read_Body);
  if (UNLIKELY(recv_buf_->length() < body_length_)) {
    VLOG(3) << "DoT Response Expected Data: " << body_length_ << " bytes Current: " << recv_buf_->length() << " bytes";

    scoped_refptr<DoTRequest> self(this);
    recv_buf_->reserve(0, body_length_ - recv_buf_->length());
    ssl_socket_->WaitRead([this, self](asio::error_code ec) { OnSSLReadable(ec); });
    return;
  }

  OnParseDnsResponse();
}

void DoTRequest::OnParseDnsResponse() {
  DCHECK_EQ(read_state_, Read_Body);

  dns_message::response_parser response_parser;
  dns_message::response response;

  dns_message::response_parser::result_type result;
  std::tie(result, std::ignore) =
      response_parser.parse(response, recv_buf_->data(), recv_buf_->data(), recv_buf_->data() + recv_buf_->length());
  if (result != dns_message::response_parser::good) {
    LOG(WARNING) << "DoT Response Bad Format";
    OnDoneRequest(asio::error::operation_not_supported, {});
    return;
  }
  VLOG(3) << "DoT Response Body Parsed: " << recv_buf_->length() << " bytes";
  recv_buf_->clear();

  struct addrinfo* addrinfo = addrinfo_dup(dns_type_ == dns_message::DNS_TYPE_AAAA, response, port_);

  OnDoneRequest({}, addrinfo);
}

void DoTRequest::OnDoneRequest(asio::error_code ec, struct addrinfo* addrinfo) {
  if (auto cb = std::move(cb_)) {
    cb(ec, addrinfo);
  } else {
    addrinfo_freedup(addrinfo);
  }
}

}  // namespace net
