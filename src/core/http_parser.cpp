// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "core/http_parser.hpp"

static void SplitHostPort(std::string *out_hostname, std::string *out_port,
                          const std::string &hostname_and_port) {
  size_t colon_offset = hostname_and_port.find_last_of(':');
  const size_t bracket_offset = hostname_and_port.find_last_of(']');
  std::string hostname, port;

  // An IPv6 literal may have colons internally, guarded by square brackets.
  if (bracket_offset != std::string::npos &&
      colon_offset != std::string::npos && bracket_offset > colon_offset) {
    colon_offset = std::string::npos;
  }

  if (colon_offset == std::string::npos) {
    *out_hostname = hostname_and_port;
    *out_port = "80";
  } else {
    *out_hostname = hostname_and_port.substr(0, colon_offset);
    *out_port = hostname_and_port.substr(colon_offset + 1);
  }
}

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
    std::string authority = std::string(buf, len);
    std::string hostname, port;
    SplitHostPort(&hostname, &port, authority);

    // Handle IPv6 literals.
    if (hostname.size() >= 2 && hostname[0] == '[' &&
        hostname[hostname.size() - 1] == ']') {
      hostname = hostname.substr(1, hostname.size() - 2);
    }

    char* end;
    const unsigned long portnum = strtoul(port.c_str(), &end, 10);
    if (*end != '\0' || portnum > UINT16_MAX || (errno == ERANGE && portnum == ULONG_MAX)) {
      VLOG(1) << "parser failed: bad http field: Host: " << authority
        << " hostname: " << hostname << " port: " << port;
      return -1;
    }
    self->http_host_ = hostname;
    self->http_port_ = portnum;
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
