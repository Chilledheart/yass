//
// socks5_request_parser.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CORE_SOCKS5_REQUEST_PARSER
#define H_CORE_SOCKS5_REQUEST_PARSER

#include <boost/system/error_code.hpp>
#include <cstdlib>
#include <glog/logging.h>

#include "socks5_request.hpp"

namespace socks5 {
class method_select_request;
class method_select_request_parser {
public:
  /// Construct ready to parse the request method.
  method_select_request_parser();

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
  parse(method_select_request &req, InputIterator begin, InputIterator end) {
    InputIterator i = begin;
    switch (state_) {
    case request_start:
      if (end - i < (int)sizeof(method_select_request_header)) {
        return std::make_tuple(indeterminate, i);
      }
      memcpy(&req.req_, &*i, sizeof(method_select_request_header));
      if (req.ver() != version) {
        return std::make_tuple(bad, i);
      }
      i += sizeof(method_select_request_header);
      state_ = request;
      return parse(req, i, end);
    case request: {
      bool auth_checked = false;
      if (end - i < (int)req.nmethods() * (int)sizeof(uint8_t)) {
        return std::make_tuple(indeterminate, i);
      }
      memcpy(&req.methods_, &*i, req.nmethods() * sizeof(uint8_t));
      for (uint32_t i = 0; i < req.nmethods(); ++i) {
        if (req.methods_[i] == no_auth_required) {
          auth_checked = true;
        }
      }
      if (!auth_checked) {
        return std::make_tuple(bad, i);
      }
      i += req.nmethods() * sizeof(uint8_t);
      return std::make_tuple(good, i);
    }
    }
    return std::make_tuple(indeterminate, begin);
  }

private:
  enum state {
    request_start,
    request,
  } state_;
};

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
    case request_start:
      if (end - i < (int)sizeof(request_header)) {
        return std::make_tuple(indeterminate, i);
      }
      memcpy(&req.req_, &*i, sizeof(request_header));
      VLOG(2) << "socks5: anom request:" << std::hex << " ver: 0x"
              << (int)req.version() << " cmd: 0x" << (int)req.command()
              << std::dec;
      if (req.version() != version) {
        return std::make_tuple(bad, i);
      }

      i += sizeof(request_header);
      state_ = request_address_start;
      return parse(req, i, end);
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
               sizeof(boost::asio::ip::address_v4::bytes_type));
        i += sizeof(boost::asio::ip::address_v4::bytes_type);

        memcpy(&req.atyp_req_.port_high_byte, &*i, sizeof(uint8_t));
        i += sizeof(uint8_t);
        memcpy(&req.atyp_req_.port_low_byte, &*i, sizeof(uint8_t));
        i += sizeof(uint8_t);
        break;
      case domain:
        memcpy(&req.atyp_req_.domain_name_len, &*i, sizeof(uint8_t));
        if (end - i <
            (int)req.atyp_req_.domain_name_len + (int)sizeof(uint16_t)) {
          return std::make_tuple(indeterminate, i);
        }
        i += sizeof(uint8_t);

        memcpy(req.atyp_req_.domain_name, &*i, req.atyp_req_.domain_name_len);
        i += req.atyp_req_.domain_name_len;

        memcpy(&req.atyp_req_.port_high_byte, &*i, sizeof(uint8_t));
        i += sizeof(uint8_t);
        memcpy(&req.atyp_req_.port_low_byte, &*i, sizeof(uint8_t));
        i += sizeof(uint8_t);
        break;
      case ipv6:
        memcpy(&req.atyp_req_.address6, &*i,
               sizeof(boost::asio::ip::address_v6::bytes_type));
        i += sizeof(boost::asio::ip::address_v4::bytes_type);

        memcpy(&req.atyp_req_.port_high_byte, &*i, sizeof(uint8_t));
        i += sizeof(uint8_t);
        memcpy(&req.atyp_req_.port_low_byte, &*i, sizeof(uint8_t));
        i += sizeof(uint8_t);
        break;
      default:
        return std::make_tuple(bad, i);
      }

      if (req.address_type() == domain) {
        VLOG(2) << "socks5: adt: 0x" << std::hex << (int)req.address_type()
                << std::dec << " addr: " << req.domain_name();
      } else {
        VLOG(2) << "socks5: adt: 0x" << std::hex << (int)req.address_type()
                << std::dec << " addr: " << req.endpoint();
      }
      return std::make_tuple(good, i);
    }
    return std::make_tuple(bad, i);
  }

private:
  enum state {
    request_start,
    request_address_start,
  } state_;
};

} // namespace socks5

#endif // H_CORE_SOCKS5_REQUEST_PARSER
