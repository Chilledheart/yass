//
// ss_request_parser.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CORE_SS_REQUEST_PARSER
#define H_CORE_SS_REQUEST_PARSER

#include <cstdlib>

#include "core/logging.hpp"
#include "core/ss_request.hpp"

namespace ss {
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
  std::tuple<result_type, InputIterator>
  parse(request &req, InputIterator begin, InputIterator end) {
    InputIterator i = begin;
    switch (state_) {
    case request_address_start:
      if (end - i < (int)sizeof(uint8_t)) {
        return std::make_tuple(indeterminate, i);
      }
      memcpy(&req.atyp_req_.address_type, &*i, sizeof(uint8_t));
      ++i;
      if (req.address_type() != ipv4 && req.address_type() != domain &&
          req.address_type() != ipv6) {
        return std::make_tuple(bad, i);
      }
      size_t address_type_size = req.address_type_size();
      if (end - i < (int)address_type_size) {
        return std::make_tuple(indeterminate, i);
      }
      /* deal with header, variable part */
      switch (req.address_type()) {
      case ipv4:
        memcpy(&req.atyp_req_.address4, &*i,
               sizeof(asio::ip::address_v4::bytes_type));
        i += sizeof(asio::ip::address_v4::bytes_type);

        req.port_high_byte() = *i;
        i += sizeof(uint8_t);
        req.port_low_byte() = *i;
        i += sizeof(uint8_t);
        break;
      case domain:
        memcpy(&req.atyp_req_.domain.domain_name_len, &*i, sizeof(uint8_t));
        if (end - i <
            (int)req.atyp_req_.domain.domain_name_len + (int)sizeof(uint16_t)) {
          return std::make_tuple(indeterminate, i);
        }
        i += sizeof(uint8_t);

        memcpy(req.atyp_req_.domain.domain_name, &*i, req.atyp_req_.domain.domain_name_len);
        i += req.atyp_req_.domain.domain_name_len;

        req.port_high_byte() = *i;
        i += sizeof(uint8_t);
        req.port_low_byte() = *i;
        i += sizeof(uint8_t);
        break;
      case ipv6:
        memcpy(&req.atyp_req_.address6, &*i,
               sizeof(asio::ip::address_v6::bytes_type));
        i += sizeof(asio::ip::address_v4::bytes_type);

        req.port_high_byte() = *i;
        i += sizeof(uint8_t);
        req.port_low_byte() = *i;
        i += sizeof(uint8_t);
        break;
      default:
        return std::make_tuple(bad, i);
      }

      if (req.address_type() == domain) {
        VLOG(3) << "ss: adt: 0x" << std::hex << (int)req.address_type()
                << std::dec << " addr: " << req.domain_name();
      } else {
        VLOG(3) << "ss: adt: 0x" << std::hex << (int)req.address_type()
                << std::dec << " addr: " << req.endpoint();
      }
      return std::make_tuple(good, i);
    }
    return std::make_tuple(bad, i);
  }

private:
  enum state {
    request_address_start,
  } state_;
};

} // namespace ss

#endif // H_CORE_SS_REQUEST_PARSER
