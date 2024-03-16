// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_NET_SOCKS4_REQUEST
#define H_NET_SOCKS4_REQUEST

#include "net/protocol.hpp"
#include "net/socks4.hpp"

#include <stdint.h>

namespace net {

namespace socks4 {

// The SOCKS server checks to see whether such a request should be granted
// based on any combination of source IP address, destination IP address,
// destination port number, the userid, and information it may obtain by
// consulting IDENT, cf. RFC 1413.  If the request is granted, the SOCKS
// server makes a connection to the specified port of the destination host.
// A reply packet is sent to the client when this connection is established,
// or when the request is rejected or the operation fails.

// A server using protocol 4A must check the DSTIP in the request packet.
// If it represent address 0.0.0.x with nonzero x, the server must read
// in the domain name that the client sends in the packet. The server
// should resolve the domain name and make connection to the destination
// host if it can.
class request {
 public:
  request() : req_() {}

  uint8_t version() const { return req_.version; }
  uint8_t command() const { return req_.command; }

  const asio::ip::address_v4::bytes_type& address() const { return req_.address; }

  asio::ip::tcp::endpoint endpoint() const {
    asio::ip::address_v4 address(req_.address);
    return asio::ip::tcp::endpoint(address, port());
  }

  bool is_socks4a() const {
    uint32_t ipnum = *(uint32_t*)&req_.address;
    return (ipnum >> 24) != 0 && (ipnum & ~(255 << 24)) == 0;
  }

  uint16_t port() const {
    unsigned short port = req_.port_high_byte;
    port = (port << 8) & 0xff00;
    port = port | req_.port_low_byte;
    return port;
  }

  std::string user_id() const { return user_id_; }

  std::string domain_name() const { return domain_name_; }

  size_t length() const {
    return sizeof(req_) + user_id_.size() + sizeof(uint8_t) +
           (is_socks4a() ? (domain_name_.size() + sizeof(uint8_t)) : 0);
  }

 private:
  friend class request_parser;
  request_header req_;
  std::string user_id_;
  std::string domain_name_;  // SOCKS4A specific
};

}  // namespace socks4

}  // namespace net

#endif  // H_NET_SOCKS4_REQUEST
