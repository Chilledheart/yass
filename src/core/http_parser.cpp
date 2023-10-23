// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "core/http_parser.hpp"
#include "core/utils.hpp"

#ifndef HTTP_MAX_HEADER_SIZE
# define HTTP_MAX_HEADER_SIZE (80*1024)
#endif

// Convert plain http proxy header to http request header
//
// reforge HTTP Request Header and pretend it to buf
// including removal of Proxy-Connection header
static void ReforgeHttpRequestImpl(std::string* header,
                                   const char* method_str,
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

  ss << method_str << " "  // NOLINT(google-*)
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

#ifdef HAVE_BALSA_HTTP_PARSER
using quiche::BalsaFrameEnums;
namespace {
constexpr absl::string_view kColonSlashSlash = "://";
// Response must start with "HTTP".
constexpr char kResponseFirstByte = 'H';

#if 0
bool isFirstCharacterOfValidMethod(char c) {
  static constexpr char kValidFirstCharacters[] = {'A', 'B', 'C', 'D', 'G', 'H', 'L', 'M',
                                                   'N', 'O', 'P', 'R', 'S', 'T', 'U'};

  const auto* begin = &kValidFirstCharacters[0];
  const auto* end = &kValidFirstCharacters[ABSL_ARRAYSIZE(kValidFirstCharacters) - 1] + 1;
  return std::binary_search(begin, end, c);
}
#endif

// TODO(#21245): Skip method validation altogether when UHV method validation is
// enabled.
bool isMethodValid(absl::string_view method, bool allow_custom_methods) {
  if (allow_custom_methods) {
    // Allowed characters in method according to RFC 9110,
    // https://www.rfc-editor.org/rfc/rfc9110.html#section-5.1.
    static constexpr char kValidCharacters[] = {
        '!', '#', '$', '%', '&', '\'', '*', '+', '-', '.', '0', '1', '2', '3', '4', '5',
        '6', '7', '8', '9', 'A', 'B',  'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
        'M', 'N', 'O', 'P', 'Q', 'R',  'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '^', '_',
        '`', 'a', 'b', 'c', 'd', 'e',  'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
        'p', 'q', 'r', 's', 't', 'u',  'v', 'w', 'x', 'y', 'z', '|', '~'};
    const auto* begin = &kValidCharacters[0];
    const auto* end = &kValidCharacters[ABSL_ARRAYSIZE(kValidCharacters) - 1] + 1;

    return !method.empty() &&
           std::all_of(method.begin(), method.end(), [begin, end](absl::string_view::value_type c) {
             return std::binary_search(begin, end, c);
           });
  }

  static constexpr absl::string_view kValidMethods[] = {
      "ACL",       "BIND",    "CHECKOUT", "CONNECT", "COPY",       "DELETE",     "GET",
      "HEAD",      "LINK",    "LOCK",     "MERGE",   "MKACTIVITY", "MKCALENDAR", "MKCOL",
      "MOVE",      "MSEARCH", "NOTIFY",   "OPTIONS", "PATCH",      "POST",       "PROPFIND",
      "PROPPATCH", "PURGE",   "PUT",      "REBIND",  "REPORT",     "SEARCH",     "SOURCE",
      "SUBSCRIBE", "TRACE",   "UNBIND",   "UNLINK",  "UNLOCK",     "UNSUBSCRIBE"};

  const auto* begin = &kValidMethods[0];
  const auto* end = &kValidMethods[ABSL_ARRAYSIZE(kValidMethods) - 1] + 1;
  return std::binary_search(begin, end, method);
}

// This function is crafted to match the URL validation behavior of the http-parser library.
bool isUrlValid(absl::string_view url, bool is_connect) {
  if (url.empty()) {
    return false;
  }

  // Same set of characters are allowed for path and query.
  const auto is_valid_path_query_char = [](char c) {
    return c == 9 || c == 12 || ('!' <= c && c <= 126);
  };

  // The URL may start with a path.
  if (auto it = url.begin(); *it == '/' || *it == '*') {
    ++it;
    return std::all_of(it, url.end(), is_valid_path_query_char);
  }

  // If method is not CONNECT, parse scheme.
  if (!is_connect) {
    // Scheme must start with alpha and be non-empty.
    auto it = url.begin();
    if (!std::isalpha(*it)) {
      return false;
    }
    ++it;
    // Scheme started with an alpha character and the rest of it is alpha, digit, '+', '-' or '.'.
    const auto is_scheme_suffix = [](char c) {
      return std::isalpha(c) || std::isdigit(c) || c == '+' || c == '-' || c == '.';
    };
    it = std::find_if_not(it, url.end(), is_scheme_suffix);
    url.remove_prefix(it - url.begin());
    if (!absl::StartsWith(url, kColonSlashSlash)) {
      return false;
    }
    url.remove_prefix(kColonSlashSlash.length());
  }

  // Path and query start with the first '/' or '?' character.
  const auto is_path_query_start = [](char c) { return c == '/' || c == '?'; };

  // Divide the rest of the URL into two sections: host, and path/query/fragments.
  auto path_query_begin = std::find_if(url.begin(), url.end(), is_path_query_start);
  const absl::string_view host = url.substr(0, path_query_begin - url.begin());
  const absl::string_view path_query = url.substr(path_query_begin - url.begin());

  const auto valid_host_char = [](char c) {
    return std::isalnum(c) || c == '!' || c == '$' || c == '%' || c == '&' || c == '\'' ||
           c == '(' || c == ')' || c == '*' || c == '+' || c == ',' || c == '-' || c == '.' ||
           c == ':' || c == ';' || c == '=' || c == '@' || c == '[' || c == ']' || c == '_' ||
           c == '~';
  };

  // Match http-parser's quirk of allowing any number of '@' characters in host
  // as long as they are not consecutive.
  return std::all_of(host.begin(), host.end(), valid_host_char) && !absl::StrContains(host, "@@") &&
         std::all_of(path_query.begin(), path_query.end(), is_valid_path_query_char);
}

bool isVersionValid(absl::string_view version_input) {
  // HTTP-version is defined at
  // https://www.rfc-editor.org/rfc/rfc9112.html#section-2.3. HTTP/0.9 requests
  // have no http-version, so empty `version_input` is also accepted.

#if 0
  static const auto regex = [] {
    envoy::type::matcher::v3::RegexMatcher matcher;
    *matcher.mutable_google_re2() = envoy::type::matcher::v3::RegexMatcher::GoogleRE2();
    matcher.set_regex("|HTTP/[0-9]\\.[0-9]");
    return Regex::Utility::parseRegex(matcher);
  }();

  return regex->match(version_input);
#else
  return true;
#endif
}

} // anonymous namespace

HttpRequestParser::HttpRequestParser(bool is_request) {
  quiche::HttpValidationPolicy http_validation_policy;
  http_validation_policy.disallow_header_continuation_lines = true;
  http_validation_policy.require_header_colon = true;
  http_validation_policy.disallow_multiple_content_length = false;
  http_validation_policy.disallow_transfer_encoding_with_content_length = false;
  framer_.set_http_validation_policy(http_validation_policy);

  framer_.set_balsa_headers(&headers_);
#if 0
  if (enable_trailers) {
    framer_.set_balsa_trailer(&trailers_);
  }
#endif
  framer_.set_balsa_visitor(this);
  framer_.set_max_header_length(HTTP_MAX_HEADER_SIZE);
  framer_.set_invalid_chars_level(quiche::BalsaFrame::InvalidCharsLevel::kError);

  framer_.set_is_request(is_request);
}

int HttpRequestParser::Parse(std::shared_ptr<IOBuf> buf, bool *ok) {
  int processed = framer_.ProcessInput(reinterpret_cast<const char*>(buf->data()), buf->length());
  *ok = status_ == ParserStatus::Ok;
  return processed;
}

void HttpRequestParser::ReforgeHttpRequest(std::string *header,
                                           const absl::flat_hash_map<std::string, std::string> *additional_headers) {
  ReforgeHttpRequestImpl(header, method_.c_str(), additional_headers, http_url_, http_headers_);
}

void HttpRequestParser::OnRawBodyInput(absl::string_view /*input*/) {}

void HttpRequestParser::OnBodyChunkInput(absl::string_view /*input*/) {}

void HttpRequestParser::OnHeaderInput(absl::string_view /*input*/) {}
void HttpRequestParser::OnTrailerInput(absl::string_view /*input*/) {}
void HttpRequestParser::OnHeader(absl::string_view /*key*/, absl::string_view /*value*/) {}

void HttpRequestParser::ProcessHeaders(const quiche::BalsaHeaders& headers) {
  for (const std::pair<absl::string_view, absl::string_view>& key_value : headers.lines()) {
    absl::string_view key = key_value.first;
    absl::string_view value = key_value.second;
    http_headers_[std::string(key)] = std::string(value);
    LOG(WARNING) << key << "=" << value;

    if (key == "Host" && !http_is_connect_) {
      std::string authority = std::string(value);
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
        status_ = ParserStatus::Error;
        break;
      }
      http_host_ = hostname;
      http_port_ = portnum;
    }
  }
}

void HttpRequestParser::ProcessTrailers(const quiche::BalsaHeaders& /*trailer*/) {}

void HttpRequestParser::OnTrailers(std::unique_ptr<quiche::BalsaHeaders> /*trailers*/) {}

void HttpRequestParser::OnRequestFirstLineInput(absl::string_view /*line_input*/,
                                                absl::string_view method_input,
                                                absl::string_view request_uri,
                                                absl::string_view version_input) {
  if (status_ == ParserStatus::Error) {
    return;
  }
  if (!isMethodValid(method_input, false)) {
    status_ = ParserStatus::Error;
    error_message_ = "HPE_INVALID_METHOD";
    return;
  }
  const bool is_connect = method_input == "CONNECT";
  http_is_connect_ = is_connect;
  method_ = std::string(method_input);
  if (!isUrlValid(request_uri, is_connect)) {
    status_ = ParserStatus::Error;
    error_message_ = "HPE_INVALID_URL";
    return;
  }
  http_url_ = std::string(request_uri);
  if (is_connect) {
    std::string authority = http_url_;
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
      status_ = ParserStatus::Error;
      error_message_ = "HPE_INVALID_URL";
      return;
    }
    http_host_ = hostname;
    http_port_ = portnum;
  }
  if (!isVersionValid(version_input)) {
    status_ = ParserStatus::Error;
    error_message_ = "HPE_INVALID_VERSION";
    return;
  }
}

void HttpRequestParser::OnResponseFirstLineInput(absl::string_view /*line_input*/,
                                                 absl::string_view version_input,
                                                 absl::string_view status_input,
                                                 absl::string_view /*reason_input*/) {
  if (status_ == ParserStatus::Error) {
    return;
  }
  if (!isVersionValid(version_input)) {
    status_ = ParserStatus::Error;
    error_message_ = "HPE_INVALID_VERSION";
    return;
  }
  auto status = StringToInteger(status_input);
  if (!status.ok()) {
    LOG(WARNING) << status.status();
    status_ = ParserStatus::Error;
    error_message_ = "HPE_INVALID_STATUS";
    return;
  }
  status_code_ = status.value();
}

void HttpRequestParser::OnChunkLength(size_t /*chunk_length*/) {}

void HttpRequestParser::OnChunkExtensionInput(absl::string_view /*input*/) {}

void HttpRequestParser::OnInterimHeaders(std::unique_ptr<quiche::BalsaHeaders> /*header*/) {}

#if 0
void HttpRequestParser::OnInterimHeaders(BalsaHeaders /*headers*/) {}
#endif

void HttpRequestParser::HeaderDone() {
  if (status_ == ParserStatus::Error) {
    return;
  }
  headers_done_ = true;
}

void HttpRequestParser::ContinueHeaderDone() {}

void HttpRequestParser::MessageDone() {
  if (status_ == ParserStatus::Error) {
    return;
  }
  framer_.Reset();
  first_byte_processed_ = false;
  headers_done_ = false;
}

void HttpRequestParser::HandleError(BalsaFrameEnums::ErrorCode error_code) {
  status_ = ParserStatus::Error;
  switch (error_code) {
  case BalsaFrameEnums::UNKNOWN_TRANSFER_ENCODING:
    error_message_ = "unsupported transfer encoding";
    break;
  case BalsaFrameEnums::INVALID_CHUNK_LENGTH:
    error_message_ = "HPE_INVALID_CHUNK_SIZE";
    break;
  case BalsaFrameEnums::HEADERS_TOO_LONG:
    error_message_ = "headers size exceeds limit";
    break;
  case BalsaFrameEnums::TRAILER_TOO_LONG:
    error_message_ = "trailers size exceeds limit";
    break;
  case BalsaFrameEnums::TRAILER_MISSING_COLON:
    error_message_ = "HPE_INVALID_HEADER_TOKEN";
    break;
  case BalsaFrameEnums::INVALID_HEADER_CHARACTER:
    error_message_ = "header value contains invalid chars";
    break;
  default:
    error_message_ = BalsaFrameEnums::ErrorCodeToString(error_code);
  }
}

void HttpRequestParser::HandleWarning(BalsaFrameEnums::ErrorCode error_code) {
  if (error_code == BalsaFrameEnums::TRAILER_MISSING_COLON) {
    HandleError(error_code);
  }
}

HttpResponseParser::HttpResponseParser() : HttpRequestParser(false) {}

#else
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

void HttpRequestParser::ReforgeHttpRequest(std::string *header,
                        const absl::flat_hash_map<std::string, std::string> *additional_headers) {
  ReforgeHttpRequestImpl(header, http_method_str((http_method)parser_.method), additional_headers, http_url_, http_headers_);
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

#endif // HAVE_BALSA_HTTP_PARSER
