//
// ss.hpp
// ~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_SS
#define H_SS

#include <cstdint>
#include <glog/logging.h>

namespace ss {

//
// Shadowsocks TCP Relay Header:
//
//    +------+----------+----------+
//    | ATYP | DST.ADDR | DST.PORT |
//    +------+----------+----------+
//    |  1   | Variable |    2     |
//    +------+----------+----------+
//
//
// In an address field (DST.ADDR, BND.ADDR), the ATYP field specifies
//    the type of address contained within the field:
//
//           o  X'01'
//
//    the address is a version-4 IP address, with a length of 4 octets
//
//           o  X'03'
//
//    the address field contains a fully-qualified domain name.  The first
//    octet of the address field contains the number of octets of name that
//    follow, there is no terminating NUL octet.
//
//           o  X'04'
//
//    the address is a version-6 IP address, with a length of 16 octets.
enum address_type {
  ipv4 = 0x01,
  domain = 0x03,
  ipv6 = 0x04,
};

struct request_header {
  uint8_t address_type;
};

class request {
public:
  request(const std::string &domain_name, uint16_t port) : buffer_() {
    uint8_t *i = buffer_;
    *i++ = domain;
    DCHECK_LE(domain_name.size(), uint8_t(~0));

    *i++ = domain_name.size();

    memcpy(i, domain_name.c_str(), domain_name.size());
    i += domain_name.size();

    port = htons(port);
    memcpy(i, &port, sizeof(port));
    i += sizeof(port);

    length_ = i - buffer_;
  }

  request(const boost::asio::ip::tcp::endpoint &endpoint) : buffer_() {
    boost::asio::ip::address addr = endpoint.address();
    uint16_t port = endpoint.port();
    uint8_t *i = buffer_;
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

    length_ = i - buffer_;
  }

  const uint8_t *data() { return buffer_; }
  size_t length() const { return length_; }

private:
  uint8_t buffer_[1 + 255 + 2];
  size_t length_;
  friend class request_parser;
};

} // namespace ss

#endif // H_SS
