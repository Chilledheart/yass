// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_HTTP_PARSER_HPP
#define H_HTTP_PARSER_HPP

#include <absl/container/flat_hash_map.h>

#include "core/iobuf.hpp"

#ifdef HAVE_BALSA_HTTP_PARSER
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wdeprecated"
#endif  // defined(__clang__)
#include <absl/base/attributes.h>
#include <quiche/balsa/balsa_enums.h>
#include <quiche/balsa/balsa_frame.h>
#include <quiche/balsa/balsa_headers.h>
#include <quiche/balsa/balsa_visitor_interface.h>
#if defined(__clang__)
#pragma GCC diagnostic pop
#endif  // defined(__clang__)
#else
#include <http_parser.h>
#endif

#ifdef HAVE_BALSA_HTTP_PARSER
// ParserStatus represents the internal state of the parser.
enum class ParserStatus {
  // An error has occurred.
  Error = -1,
  // No error.
  Ok = 0,
  // The parser is paused.
  Paused,
};
class HttpRequestParser : public quiche::BalsaVisitorInterface {
 public:
  HttpRequestParser(bool is_request = true);

  int Parse(std::shared_ptr<IOBuf> buf, bool *ok);

  const std::string& host() const { return http_host_; }
  uint16_t port() const { return http_port_; }
  bool is_connect() const { return http_is_connect_; }

  void ReforgeHttpRequest(std::string *header,
                          const absl::flat_hash_map<std::string, std::string> *additional_headers = nullptr);

  const char* ErrorMessage() {
    return error_message_.empty() ? "" : error_message_.data();
  }

  int status_code() const { return status_code_; }

 protected:
  // quiche::BalsaVisitorInterface implementation
  // TODO(bnc): Encapsulate in a private object.
  void OnRawBodyInput(absl::string_view input) override;
  void OnBodyChunkInput(absl::string_view input) override;
  void OnHeaderInput(absl::string_view input) override;
  void OnHeader(absl::string_view key, absl::string_view value) override;
  void OnTrailerInput(absl::string_view input) override;
  void ProcessHeaders(const quiche::BalsaHeaders& headers) override;
  void ProcessTrailers(const quiche::BalsaHeaders& trailer) override;
  void OnTrailers(std::unique_ptr<quiche::BalsaHeaders> trailers) override;
  void OnRequestFirstLineInput(absl::string_view line_input, absl::string_view method_input,
                               absl::string_view request_uri,
                               absl::string_view version_input) override;
  void OnResponseFirstLineInput(absl::string_view line_input, absl::string_view version_input,
                                absl::string_view status_input,
                                absl::string_view reason_input) override;
  void OnChunkLength(size_t chunk_length) override;
  void OnChunkExtensionInput(absl::string_view input) override;
  void OnInterimHeaders(std::unique_ptr<quiche::BalsaHeaders> header) override;
  void HeaderDone() override;
  void ContinueHeaderDone() override;
  void MessageDone() override;
  void HandleError(quiche::BalsaFrameEnums::ErrorCode error_code) override;
  void HandleWarning(quiche::BalsaFrameEnums::ErrorCode error_code) override;

  quiche::BalsaFrame framer_;
  quiche::BalsaHeaders headers_;

  /// copy of method
  std::string method_;
  /// copy of url
  std::string http_url_;
  /// copy of parsed connect host or host field
  std::string http_host_;
  /// copy of parsed connect host or host field
  uint16_t http_port_ = 0U;
  /// copy of parsed headers
  absl::flat_hash_map<std::string, std::string> http_headers_;
  /// copy of connect method
  bool http_is_connect_ = false;

  bool first_byte_processed_ = false;
  bool headers_done_ = false;
  ParserStatus status_ = ParserStatus::Ok;
  int status_code_ = 0;
  // An error message, often seemingly arbitrary to match http-parser behavior.
  absl::string_view error_message_;
};

class HttpResponseParser : public HttpRequestParser {
  public:
    HttpResponseParser();
};
#else
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
                          const absl::flat_hash_map<std::string, std::string> *additional_headers = nullptr);

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
#endif // HAVE_BALSA_HTTP_PARSER

#endif // H_HTTP_PARSER_HPP
