// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_HTTP_PARSER_HPP
#define H_HTTP_PARSER_HPP

#include <absl/container/flat_hash_map.h>

#include "core/iobuf.hpp"
#include "http_parser/http_parser.h"

class HttpRequestParser {
 public:
  HttpRequestParser() : parser_() {
    ::http_parser_init(&parser_, HTTP_REQUEST);
    parser_.data = this;
  }

  int Parse(std::shared_ptr<IOBuf> buf, bool *ok) {
    struct http_parser_settings settings_connect = {
        //.on_message_begin
        nullptr,
        //.on_url
        &HttpRequestParser::OnReadHttpRequestURL,
        //.on_status
        nullptr,
        //.on_header_field
        &HttpRequestParser::OnReadHttpRequestHeaderField,
        //.on_header_value
        &HttpRequestParser::OnReadHttpRequestHeaderValue,
        //.on_headers_complete
        HttpRequestParser::OnReadHttpRequestHeadersDone,
        //.on_body
        nullptr,
        //.on_message_complete
        nullptr,
        //.on_chunk_header
        nullptr,
        //.on_chunk_complete
        nullptr};
    size_t nparsed;
    nparsed = http_parser_execute(&parser_, &settings_connect,
                                  reinterpret_cast<const char*>(buf->data()),
                                  buf->length());
    *ok = HTTP_PARSER_ERRNO(&parser_) == HPE_OK;
    return nparsed;
  }

  void ReforgeHttpRequest(std::string *header) {
    ReforgeHttpRequestImpl(header, &parser_, http_url_, http_headers_);
  }

  const char* ErrorMessage() {
    return http_errno_description(HTTP_PARSER_ERRNO(&parser_));
  }

  const std::string& host() const { return http_host_; }
  uint16_t port() const { return http_port_; }
  bool is_connect() const { return http_is_connect_; }

 private:
  /// Callback to read http handshake request's URL field
  static int OnReadHttpRequestURL(http_parser* p, const char* buf, size_t len);
  /// Callback to read http handshake request's URL field
  static int OnReadHttpRequestHeaderField(http_parser* parser,
                                          const char* buf,
                                          size_t len);
  /// Callback to read http handshake request's headers done
  static int OnReadHttpRequestHeaderValue(http_parser* parser,
                                          const char* buf,
                                          size_t len);
  /// Callback to read http handshake request's headers done
  static int OnReadHttpRequestHeadersDone(http_parser* parser);

  static int OnHttpRequestParseUrl(const char* buf,
                                   size_t len,
                                   std::string* host,
                                   uint16_t* port,
                                   int is_connect) {
    struct http_parser_url url;

    if (0 != ::http_parser_parse_url(buf, len, is_connect, &url)) {
      LOG(ERROR) << "Failed to parse url: '" << std::string(buf, len) << "'";
      return 1;
    }

    if (url.field_set & (1 << (UF_HOST))) {
      *host = std::string(buf + url.field_data[UF_HOST].off,
                          url.field_data[UF_HOST].len);
    }

    if (url.field_set & (1 << (UF_PORT))) {
      *port = url.port;
    }

    return 0;
  }

  // Convert plain http proxy header to http request header
  //
  // reforge HTTP Request Header and pretend it to buf
  // including removal of Proxy-Connection header
  static void ReforgeHttpRequestImpl(std::string* header, ::http_parser* p,
                                     const std::string& url,
                                     const absl::flat_hash_map<std::string, std::string>& headers) {
    std::stringstream ss;
    ss << http_method_str((http_method)p->method) << " "  // NOLINT(google-*)
       << url << " HTTP/1.1\r\n";
    for (const std::pair<std::string, std::string> pair : headers) {
      if (pair.first == "Proxy-Connection") {
        continue;
      }
      ss << pair.first << ": " << pair.second << "\r\n";
    }
    ss << "\r\n";

    *header = ss.str();
  }

 private:
  ::http_parser parser_;
  /// copy of url
  std::string http_url_;
  /// copy of parsed connect host or host field
  std::string http_host_;
  /// copy of parsed connect host or host field
  uint16_t http_port_ = 0U;
  /// copy of parsed header field
  std::string http_field_;
  /// copy of parsed header value
  std::string http_value_;
  /// copy of parsed headers
  absl::flat_hash_map<std::string, std::string> http_headers_;
  /// copy of connect method
  bool http_is_connect_ = false;
};

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


#endif // H_HTTP_PARSER_HPP
