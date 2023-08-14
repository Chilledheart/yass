// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_HTTP_PARSER_HPP
#define H_HTTP_PARSER_HPP

#include <absl/container/flat_hash_map.h>

#include "core/iobuf.hpp"
#include <http_parser.h>

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

  void ReforgeHttpRequest(std::string *header,
                          const absl::flat_hash_map<std::string, std::string> *additional_headers = nullptr) {
    ReforgeHttpRequestImpl(header, &parser_, additional_headers, http_url_, http_headers_);
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
  /// Callback to read http handshake request's header field
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
                                     const absl::flat_hash_map<std::string, std::string> *additional_headers,
                                     const std::string& url,
                                     const absl::flat_hash_map<std::string, std::string>& headers) {
    std::ostringstream ss;
    absl::string_view canon_url;

    // https://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html#sec5.1.2
    if (url[0] == '*' || url[0] == '/') {
      // allow all defined in spec except for absoluteURI
      canon_url = url;
    } else {
      // convert absoluteURI to abs_path
      auto absolute_prefix = url.find("://");
      if (absolute_prefix == std::string::npos) {
        LOG(WARNING) << "Invalid Uri: " << url;
        canon_url = url;
      } else {
        auto uri_start = url.find('/', absolute_prefix + 3);
        if (uri_start == std::string::npos) {
          canon_url = "/";
        } else {
          canon_url = absl::string_view(url).substr(uri_start);
        }
      }
    }

    ss << http_method_str((http_method)p->method) << " "  // NOLINT(google-*)
       << canon_url << " HTTP/1.1\r\n";
    for (auto [key, value] : headers) {
      if (key == "Proxy-Connection") {
        continue;
      }
      ss << key << ": " << value << "\r\n";
    }
    if (additional_headers) {
      for (auto [key, value] : *additional_headers) {
        ss << key << ": " << value << "\r\n";
      }
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

class HttpResponseParser {
 public:
  HttpResponseParser() : parser_() {
    ::http_parser_init(&parser_, HTTP_RESPONSE);
    parser_.data = this;
  }

  int Parse(std::shared_ptr<IOBuf> buf, bool *ok) {
    struct http_parser_settings settings_connect = {
        //.on_message_begin
        nullptr,
        //.on_url
        nullptr,
        //.on_status
        nullptr,
        //.on_header_field
        &HttpResponseParser::OnReadHttpResponseHeaderField,
        //.on_header_value
        &HttpResponseParser::OnReadHttpResponseHeaderValue,
        //.on_headers_complete
        HttpResponseParser::OnReadHttpResponseHeadersDone,
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

  const char* ErrorMessage() {
    return http_errno_description(HTTP_PARSER_ERRNO(&parser_));
  }

  int status_code() const { return parser_.status_code; }

 private:
  /// Callback to read http handshake request's header field
  static int OnReadHttpResponseHeaderField(http_parser* parser,
                                           const char* buf,
                                           size_t len);
  /// Callback to read http handshake request's headers done
  static int OnReadHttpResponseHeaderValue(http_parser* parser,
                                           const char* buf,
                                           size_t len);
  /// Callback to read http handshake request's headers done
  static int OnReadHttpResponseHeadersDone(http_parser* parser);

 private:
  ::http_parser parser_;
  /// copy of parsed header field
  std::string http_field_;
  /// copy of parsed header value
  std::string http_value_;
  /// copy of parsed headers
  absl::flat_hash_map<std::string, std::string> http_headers_;
};

#endif // H_HTTP_PARSER_HPP
