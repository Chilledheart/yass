// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "core/http_parser.hpp"

int HttpRequestParser::OnReadHttpRequestURL(http_parser* p,
                                            const char* buf,
                                            size_t len) {
  HttpRequestParser* self = reinterpret_cast<HttpRequestParser*>(p->data);
  self->http_url_ = std::string(buf, len);
  if (p->method == HTTP_CONNECT) {
    if (0 != OnHttpRequestParseUrl(buf, len, &self->http_host_,
                                   &self->http_port_, 1)) {
      return 1;
    }
    self->http_is_connect_ = true;
  }
  return 0;
}

int HttpRequestParser::OnReadHttpRequestHeaderField(http_parser* parser,
                                                    const char* buf,
                                                    size_t len) {
  HttpRequestParser* self = reinterpret_cast<HttpRequestParser*>(parser->data);
  self->http_field_ = std::string(buf, len);
  return 0;
}

int HttpRequestParser::OnReadHttpRequestHeaderValue(http_parser* parser,
                                                    const char* buf,
                                                    size_t len) {
  HttpRequestParser* self = reinterpret_cast<HttpRequestParser*>(parser->data);
  self->http_value_ = std::string(buf, len);
  self->http_headers_[self->http_field_] = self->http_value_;
  if (self->http_field_ == "Host" && !self->http_is_connect_) {
    const char* url = buf;
    // Host = "Host" ":" host [ ":" port ] ; Section 3.2.2
    // TBD hand with IPv6 address // [xxx]:port/xxx
    while (*buf != ':' && *buf != '\0' && len != 0) {
      buf++, len--;
    }

    self->http_host_ = std::string(url, buf);
    if (len > 1 && *buf == ':') {
      ++buf, --len;
      self->http_port_ = stoi(std::string(buf, len));
    } else {
      self->http_port_ = 80;
    }
  }
  return 0;
}

int HttpRequestParser::OnReadHttpRequestHeadersDone(http_parser*) {
  // Treat the rest part as Upgrade even when it is not CONNECT
  // (binary protocol such as ocsp-request and dns-message).
  return 2;
}

int HttpResponseParser::OnReadHttpResponseHeaderField(http_parser* parser,
                                                      const char* buf,
                                                      size_t len) {
  HttpResponseParser* self = reinterpret_cast<HttpResponseParser*>(parser->data);
  self->http_field_ = std::string(buf, len);
  return 0;
}

int HttpResponseParser::OnReadHttpResponseHeaderValue(http_parser* parser,
                                                      const char* buf,
                                                      size_t len) {
  HttpResponseParser* self = reinterpret_cast<HttpResponseParser*>(parser->data);
  self->http_value_ = std::string(buf, len);
  self->http_headers_[self->http_field_] = self->http_value_;
  LOG(WARNING) << self->http_value_ << "=" << self->http_value_;
  return 0;
}

int HttpResponseParser::OnReadHttpResponseHeadersDone(http_parser*) {
  // Treat the rest part as Upgrade even when it is not CONNECT
  // (binary protocol such as ocsp-request and dns-message).
  return 2;
}
