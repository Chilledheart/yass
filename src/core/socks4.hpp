// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef SOCKS4_HPP
#define SOCKS4_HPP

#include "protocol.hpp"

#include <array>
#include <stdint.h>
#include <string>

namespace socks4 {

// see also: https://www.openssh.com/txt/socks4.protocol
// VN is the SOCKS protocol version number and should be 4.
const uint8_t version = 0x04;

#ifdef __GNUC__
#define PACK(__Declaration__)                                                  \
  __Declaration__ __attribute__((packed, aligned(1)))
#endif

#ifdef _MSC_VER
#define PACK(__Declaration__)                                                  \
  __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

// CD is the SOCKS command code and should be 1 for CONNECT or 2 for BIND.
// NULL is a byte of all zero bits.
enum command_type {
  cmd_connect = 0x01,
  cmd_bind = 0x02,
};

// +----+----+----+----+----+----+----+----+----+----+....+----+
// | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
// +----+----+----+----+----+----+----+----+----+----+....+----+
//    1    1      2              4           variable       1
PACK(struct request_header {
  uint8_t version;
  uint8_t command;
  uint8_t port_high_byte;
  uint8_t port_low_byte;
  asio::ip::address_v4::bytes_type address;
});

// +----+----+----+----+----+----+----+----+
// | VN | CD | DSTPORT |      DSTIP        |
// +----+----+----+----+----+----+----+----+
//    1    1      2              4
//	90: request granted
//  91: request rejected or failed
//  92: request rejected becasue SOCKS server cannot connect to
//      identd on the client
//  93: request rejected because the client program and identd
//      report different user-ids
class reply {
public:
  enum status_type {
    request_granted = 0x5a,
    request_failed = 0x5b,
    request_failed_no_identd = 0x5c,
    request_failed_bad_user_id = 0x5d
  };

  reply() : null_byte_(0), status_() {}

  std::array<asio::mutable_buffer, 5> buffers() {
    return {{asio::buffer(&null_byte_, 1), asio::buffer(&status_, 1),
             asio::buffer(&port_high_byte_, 1),
             asio::buffer(&port_low_byte_, 1), asio::buffer(address_)}};
  }

  bool success() const { return null_byte_ == 0 && status_ == request_granted; }

  uint8_t status() const { return status_; }
  uint8_t &mutable_status() { return status_; }

  asio::ip::tcp::endpoint endpoint() const {
    unsigned short port = port_high_byte_;
    port = (port << 8) & 0xff00;
    port = port | port_low_byte_;

    asio::ip::address_v4 address(address_);

    return asio::ip::tcp::endpoint(address, port);
  }

  void set_endpoint(const asio::ip::tcp::endpoint &endpoint) {
    address_ = endpoint.address().to_v4().to_bytes();

    // Convert port number to network byte order.
    unsigned short port = endpoint.port();
    port_high_byte_ = (port >> 8) & 0xff;
    port_low_byte_ = port & 0xff;
  }

  const asio::ip::address_v4::bytes_type &address() const { return address_; }

  size_t length() const { return sizeof(uint8_t) * 4 + sizeof(address_); }

  uint16_t port() const {
    unsigned short port = port_high_byte_;
    port = (port << 8) & 0xff00;
    port = port | port_low_byte_;
    return port;
  }

private:
  uint8_t null_byte_;
  uint8_t status_;
  uint8_t port_high_byte_;
  uint8_t port_low_byte_;
  asio::ip::address_v4::bytes_type address_;
};

#undef PACK

} // namespace socks4

#endif // SOCKS4_HPP
