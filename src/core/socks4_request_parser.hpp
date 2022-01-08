// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CORE_SOCKS4_REQUEST_PARSER
#define H_CORE_SOCKS4_REQUEST_PARSER

#include <cstdlib>

#include "core/logging.hpp"
#include "core/socks4_request.hpp"

namespace socks4 {
class request;
class request_parser {
 public:
  /// Construct ready to parse the request method.
  request_parser();

  /// Reset to initial parser state.
  void reset();

  /// Result of parse.
  enum result_type { good, bad, indeterminate };

  /// Parse some data. The enum return value is good when a complete request has
  /// been parsed, bad if the data is invalid, indeterminate when more data is
  /// required. The InputIterator return value indicates how much of the input
  /// has been consumed.
  template <typename InputIterator>
  std::tuple<result_type, InputIterator> parse(request& req,
                                               InputIterator begin,
                                               InputIterator end) {
    InputIterator i = begin;
    switch (state_) {
      case request_start:
        if (end - i < (int)sizeof(request_header)) {
          return std::make_tuple(indeterminate, i);
        }
        memcpy(&req.req_, &*i, sizeof(request_header));
        VLOG(3) << "socks4: anom request:" << std::hex << " ver: 0x"
                << (int)req.version() << " cmd: 0x" << (int)req.command()
                << std::dec << " addr: " << req.endpoint()
                << " is_socks4a: " << std::boolalpha << req.is_socks4a()
                << std::dec;
        if (req.version() != version) {
          return std::make_tuple(bad, i);
        }

        i += sizeof(request_header);
        state_ = request_userid_start;
        return parse(req, i, end);
      case request_userid_start:
        while (i != end && *i != '\0') {
          ++i;
        }
        if (i == end) {
          return std::make_tuple(indeterminate, i);
        }
        VLOG(3) << "socks4: user id: " << std::hex << (int)*begin;
        req.user_id_ = std::string(begin, i);
        ++i;
        if (req.is_socks4a()) {
          state_ = request_domain_start;
          return parse(req, i, end);
        }
        break;
      case request_domain_start:
        while (i != end && *i != '\0') {
          ++i;
        }
        if (i == end) {
          return std::make_tuple(indeterminate, i);
        }
        VLOG(3) << "socks4: domain_name: " << begin;
        req.domain_name_ = std::string(begin, i);
        ++i;
        break;
      default:
        return std::make_tuple(bad, i);
    }
    return std::make_tuple(good, i);
  }

 private:
  enum state {
    request_start,
    request_userid_start,
    request_domain_start,
  } state_;
};

}  // namespace socks4

#endif  // H_CORE_SOCKS4_REQUEST_PARSER
