// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CORE_SOCKS5_REQUEST
#define H_CORE_SOCKS5_REQUEST

#include "core/socks5.hpp"
#include "protocol.hpp"

#include <stdint.h>
namespace socks5 {

class method_select_request {
 public:
  method_select_request() : req_() {}

  uint8_t ver() const { return req_.ver; }
  uint8_t nmethods() const { return req_.nmethods; }

  size_t length() const { return sizeof(req_) + req_.nmethods; }

 private:
  friend class method_select_request_parser;
  method_select_request_header req_;
  uint8_t methods_[255];
};

class request {
 public:
  request() : req_(), atyp_req_() {}

  uint8_t version() const { return req_.version; }
  uint8_t command() const { return req_.command; }
  uint8_t address_type() const { return atyp_req_.address_type; }

  size_t address_type_size() const {
    switch (address_type()) {
      case ipv4:
        return sizeof(asio::ip::address_v4::bytes_type) + sizeof(uint16_t);
      case domain:
        return sizeof(uint8_t) + atyp_req_.domain.domain_name_len +
               sizeof(uint16_t);
      case ipv6:
        return sizeof(asio::ip::address_v6::bytes_type) + sizeof(uint16_t);
      default:
        return 0;
    }
  }

  size_t length() const {
    return sizeof(req_) + sizeof(uint8_t) + address_type_size();
  }

  asio::ip::tcp::endpoint endpoint() const {
    asio::ip::tcp::endpoint endpoint;
    if (address_type() == ipv4) {
      asio::ip::address_v4 address(atyp_req_.address4);
      endpoint = asio::ip::tcp::endpoint(address, port());
    } else if (address_type() == ipv6) {
      asio::ip::address_v6 address(atyp_req_.address6);
      endpoint = asio::ip::tcp::endpoint(address, port());
    }
    return endpoint;
  }

  const asio::ip::address_v4::bytes_type& address4() const {
    return atyp_req_.address4;
  }

  const asio::ip::address_v6::bytes_type& address6() const {
    return atyp_req_.address6;
  }

  std::string domain_name() const {
    return std::string((char*)atyp_req_.domain.domain_name,
                       atyp_req_.domain.domain_name_len);
  }

  uint16_t port() const {
    unsigned short port = atyp_req_.port_high_byte;
    port = (port << 8) & 0xff00;
    port = port | atyp_req_.port_low_byte;
    return port;
  }

 private:
  friend class request_parser;
  request_header req_;
  address_type_header atyp_req_;
};
}  // namespace socks5
#endif  // H_CORE_SOCKS5_REQUEST
