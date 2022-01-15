// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_SS
#define H_SS

#include <stdint.h>

#include "core/asio.hpp"
#include "core/logging.hpp"

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

// +------+----------+----------+
// | ATYP | DST.ADDR | DST.PORT |
// +------+----------+----------+
// |  1   | Variable |    2     |
// +------+----------+----------+
//
//  ATYP   address type of following address
//  o  IP V4 address: X'01'
//  o  DOMAINNAME: X'03'
//  o  IP V6 address: X'04'
//  DST.ADDR       desired destination address
//  DST.PORT desired destination port in network octet
//  order
struct address_type_domain_header {
  uint8_t domain_name_len;
  uint8_t domain_name[255];
};
struct address_type_header {
  uint8_t address_type;
  union {
    asio::ip::address_v4::bytes_type address4;
    asio::ip::address_v6::bytes_type address6;
    address_type_domain_header domain;
  };
  uint8_t port_high_byte;
  uint8_t port_low_byte;
};

}  // namespace ss

#endif  // H_SS
