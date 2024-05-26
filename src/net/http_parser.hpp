// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#ifndef H_NET_HTTP_PARSER_HPP
#define H_NET_HTTP_PARSER_HPP

#include <absl/container/flat_hash_map.h>

#include "net/iobuf.hpp"

#ifdef HAVE_BALSA_HTTP_PARSER
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  // defined(__clang__)
#include <absl/base/attributes.h>
#include <quiche/balsa/balsa_enums.h>
#include <quiche/balsa/balsa_frame.h>
#include <quiche/balsa/balsa_headers.h>
#include <quiche/balsa/balsa_visitor_interface.h>
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif  // defined(__clang__)
#else
extern "C" {
typedef struct http_parser http_parser;
}
#endif

namespace net {

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

  int Parse(std::shared_ptr<IOBuf> buf, bool* ok);

  const std::string& host() const { return http_host_; }
  uint16_t port() const { return http_port_; }
  bool is_connect() const { return http_is_connect_; }
  uint64_t content_length() const { return headers_.content_length(); }
  const std::string& content_type() const { return content_type_; }
  const std::string& connection() const { return connection_; }
  const std::string& proxy_authorization() const { return proxy_authorization_; }
  bool transfer_encoding_is_chunked() const { return headers_.transfer_encoding_is_chunked(); }

  void ReforgeHttpRequest(std::string* header,
                          const absl::flat_hash_map<std::string, std::string>* additional_headers = nullptr);

  const char* ErrorMessage() { return error_message_.empty() ? "" : error_message_.data(); }

  int status_code() const { return status_code_; }

 protected:
  // quiche::BalsaVisitorInterface implementation
  // TODO(bnc): Encapsulate in a private object.
  void OnRawBodyInput(std::string_view input) override;
  void OnBodyChunkInput(std::string_view input) override;
  void OnHeaderInput(std::string_view input) override;
  void OnTrailerInput(std::string_view input) override;
  void ProcessHeaders(const quiche::BalsaHeaders& headers) override;
  void OnTrailers(std::unique_ptr<quiche::BalsaHeaders> trailers) override;
  void OnRequestFirstLineInput(std::string_view line_input,
                               std::string_view method_input,
                               std::string_view request_uri,
                               std::string_view version_input) override;
  void OnResponseFirstLineInput(std::string_view line_input,
                                std::string_view version_input,
                                std::string_view status_input,
                                std::string_view reason_input) override;
  void OnChunkLength(size_t chunk_length) override;
  void OnChunkExtensionInput(std::string_view input) override;
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
  /// copy of version input
  std::string version_input_;
  /// copy of parsed connect host or host field
  std::string http_host_;
  /// copy of parsed connect host or host field
  uint16_t http_port_ = 0U;
  /// copy of parsed headers
  absl::flat_hash_map<std::string, std::string> http_headers_;
  /// copy of connect method
  bool http_is_connect_ = false;
  /// copy of content type
  std::string content_type_;
  /// copy of connection
  std::string connection_;
  /// copy of proxy_authorization
  std::string proxy_authorization_;

  bool first_byte_processed_ = false;
  bool headers_done_ = false;
  ParserStatus status_ = ParserStatus::Ok;
  int status_code_ = 0;
  // An error message, often seemingly arbitrary to match http-parser behavior.
  std::string_view error_message_;
};

class HttpResponseParser : public HttpRequestParser {
 public:
  HttpResponseParser();
};
#else
class HttpRequestParser {
 public:
  HttpRequestParser(bool is_request = true);
  virtual ~HttpRequestParser();

  int Parse(std::shared_ptr<IOBuf> buf, bool* ok);

  void ReforgeHttpRequest(std::string* header,
                          const absl::flat_hash_map<std::string, std::string>* additional_headers = nullptr);

  const char* ErrorMessage() const;

  const std::string& host() const { return http_host_; }
  uint16_t port() const { return http_port_; }
  bool is_connect() const { return http_is_connect_; }
  uint64_t content_length() const;
  const std::string& content_type() const { return content_type_; }
  std::string_view connection() const;
  const std::string& proxy_authorization() const { return proxy_authorization_; }
  bool transfer_encoding_is_chunked() const;

  int status_code() const;

 private:
  /// Callback to read http handshake request's URL field
  static int OnReadHttpRequestURL(http_parser* p, const char* buf, size_t len);
  /// Callback to read http handshake request's header field
  static int OnReadHttpRequestHeaderField(http_parser* parser, const char* buf, size_t len);
  /// Callback to read http handshake request's headers done
  static int OnReadHttpRequestHeaderValue(http_parser* parser, const char* buf, size_t len);
  /// Callback to read http handshake request's headers done
  static int OnReadHttpRequestHeadersDone(http_parser* parser);

 protected:
  ::http_parser* parser_ = nullptr;
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
  /// copy of content type
  std::string content_type_;
  /// copy of proxy_authorization
  std::string proxy_authorization_;
};

class HttpResponseParser : public HttpRequestParser {
 public:
  HttpResponseParser();
  ~HttpResponseParser() override {}
};
#endif  // HAVE_BALSA_HTTP_PARSER

}  // namespace net

#endif  // H_NET_HTTP_PARSER_HPP
