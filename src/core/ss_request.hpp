//
// ss_request.hpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CORE_SS_REQUEST
#define H_CORE_SS_REQUEST

#include "protocol.hpp"
#include "ss.hpp"

#include <cstdint>
namespace ss {

class request {
public:
  request() : atyp_req_() {}
  request(const std::string &domain_name, uint16_t port) {
    uint8_t *i = data();
    *i++ = domain;
    DCHECK_LE(domain_name.size(), uint8_t(~0));

    *i++ = (uint8_t)domain_name.size();

    memcpy(i, domain_name.c_str(), domain_name.size());
    i += domain_name.size();

    port = htons(port);
    memcpy(i, &port, sizeof(port));
    i += sizeof(port);
  }

  request(const boost::asio::ip::tcp::endpoint &endpoint) {
    boost::asio::ip::address addr = endpoint.address();
    uint16_t port = endpoint.port();
    uint8_t *i = data();
    if (addr.is_v4()) {
      boost::asio::ip::address_v4::bytes_type bytes = addr.to_v4().to_bytes();
      *i++ = ipv4;
      memcpy(i, &bytes, sizeof(bytes));
      i += sizeof(bytes);
    } else if (addr.is_v6()) {
      boost::asio::ip::address_v6::bytes_type bytes = addr.to_v6().to_bytes();
      *i++ = ipv6;
      memcpy(i, &bytes, sizeof(bytes));
      i += sizeof(bytes);
    }

    port = htons(port);
    memcpy(i, &port, sizeof(port));
    i += sizeof(port);
  }

  uint8_t *data() { return reinterpret_cast<uint8_t *>(&atyp_req_); }
  const uint8_t *data() const {
    return reinterpret_cast<const uint8_t *>(&atyp_req_);
  }

  size_t length() const { return address_type_size(); }

  uint8_t address_type() const { return atyp_req_.address_type; }

  size_t address_type_size() const {
    switch (address_type()) {
    case ipv4:
      return sizeof(uint8_t) + sizeof(boost::asio::ip::address_v4::bytes_type) +
             sizeof(uint16_t);
    case domain:
      return sizeof(uint8_t) + sizeof(uint8_t) + atyp_req_.domain_name_len +
             sizeof(uint16_t);
    case ipv6:
      return sizeof(uint8_t) + sizeof(boost::asio::ip::address_v6::bytes_type) +
             sizeof(uint16_t);
    default:
      return 0;
    }
  }

  boost::asio::ip::tcp::endpoint endpoint() const {
    boost::asio::ip::tcp::endpoint endpoint;
    if (address_type() == ipv4) {
      boost::asio::ip::address_v4 address(atyp_req_.address4);
      endpoint = boost::asio::ip::tcp::endpoint(address, port());
    } else if (address_type() == ipv6) {
      boost::asio::ip::address_v6 address(atyp_req_.address6);
      endpoint = boost::asio::ip::tcp::endpoint(address, port());
    }
    return endpoint;
  }

  const boost::asio::ip::address_v4::bytes_type &address4() const {
    return atyp_req_.address4;
  }

  const boost::asio::ip::address_v6::bytes_type &address6() const {
    return atyp_req_.address6;
  }

  std::string domain_name() const {
    return std::string((char *)atyp_req_.domain_name,
                       atyp_req_.domain_name_len);
  }

  uint16_t port() const {
    const uint16_t *port = reinterpret_cast<const uint16_t *>(
        data() + length() - sizeof(uint16_t));
    return ntohs(*port);
  }

  uint8_t &port_high_byte() {
    uint8_t *port =
        reinterpret_cast<uint8_t *>(data() + length() - sizeof(uint16_t));
    return *port;
  }

  uint8_t &port_low_byte() {
    uint8_t *port = reinterpret_cast<uint8_t *>(
        data() + length() - sizeof(uint16_t) + sizeof(uint8_t));
    return *port;
  }

private:
  friend class request_parser;
  address_type_header atyp_req_;
};
} // namespace ss
#endif // H_CORE_SS_REQUEST
