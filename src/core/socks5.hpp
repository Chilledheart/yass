//
// socks5.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CORE_SOCKS5
#define H_CORE_SOCKS5

#include <array>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <string>
#include <tuple>

#include "iobuf.hpp"
#include "ss.hpp"

namespace socks5 {

// see also: https://www.ietf.org/rfc/rfc1928.txt
const uint8_t version = 0x05;

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((packed, aligned(1)))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

//  X'00' NO AUTHENTICATION REQUIRED
//  X'01' GSSAPI
//  X'02' USERNAME/PASSWORD
//  X'03' to X'7F' IANA ASSIGNED
//  X'80' to X'FE' RESERVED FOR PRIVATE METHODS
//  X'FF' NO ACCEPTABLE METHODS
enum method_select {
  no_auth_required = 0x00,
  unacceptable = 0xff,
};

// +----+----------+----------+
// |VER | NMETHODS | METHODS  |
// +----+----------+----------+
// | 1  |    1     | 1 to 255 |
// +----+----------+----------+
PACK(struct method_select_request_header {
  uint8_t ver;
  uint8_t nmethods;
});

// +----+--------+
// |VER | METHOD |
// +----+--------+
// | 1  |   1    |
// +----+--------+
PACK(struct method_select_response {
  uint8_t ver;
  uint8_t method;
});

inline method_select_response
method_select_response_stock_reply(uint8_t method = no_auth_required) {
  method_select_response resp;
  resp.ver = version;
  resp.method = method;
  return resp;
}

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

enum command_type {
  cmd_connect = 0x01,
  cmd_bind = 0x02,
  cmd_udp_associate = 0x03,
};

// +----+-----+-------+
// |VER | CMD |  RSV  |
// +----+-----+-------+
// | 1  |  1  | X'00' |
// +----+-----+-------+
//
//  VER    protocol version: X'05'
//  CMD
//  o  CONNECT X'01'
//  o  BIND X'02'
//  o  UDP ASSOCIATE X'03'
//  RSV    RESERVED
PACK(struct request_header {
  uint8_t version;
  uint8_t command;
  uint8_t null_byte;
});

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
    boost::asio::ip::address_v4::bytes_type address4;
    boost::asio::ip::address_v6::bytes_type address6;
    address_type_domain_header domain;
  };
  uint8_t port_high_byte;
  uint8_t port_low_byte;
};

// +----+-----+-------+------+----------+----------+
// |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
// +----+-----+-------+------+----------+----------+
// | 1  |  1  | X'00' |  1   | Variable |    2     |
// +----+-----+-------+------+----------+----------+
//   o  VER    protocol version: X'05'
//   o  REP    Reply field:
//      o  X'00' succeeded
//      o  X'01' general SOCKS server failure
//      o  X'02' connection not allowed by ruleset
//      o  X'03' Network unreachable
//      o  X'04' Host unreachable
//      o  X'05' Connection refused
//      o  X'06' TTL expired
//      o  X'07' Command not supported
//      o  X'08' Address type not supported
//      o  X'09' to X'FF' unassigned
//   o  RSV    RESERVED
//   o  ATYP   address type of following address
class reply {
public:
  enum status_type {
    request_granted = 0x00,
    request_failed = 0x01,
    request_failed_no_identd = 0x02,
    request_failed_network_unreachable = 0x03,
    request_failed_host_unreachable = 0x04,
    request_failed_conn_refused = 0x05,
    request_failed_ttl_expired = 0x06,
    request_failed_cmd_not_supported = 0x07,
    request_failed_address_type_not_supported = 0x08,
    request_failed_ff_unassigned = 0x09,
  };

  reply() : version_(version), status_(), null_byte_(0), address_type_(0) {}

  std::array<boost::asio::mutable_buffer, 7> buffers() {
    if (address_type_ == address_type::ipv6) {
      return {{
          boost::asio::buffer(&version_, 1),
          boost::asio::buffer(&status_, 1),
          boost::asio::buffer(&null_byte_, 1),
          boost::asio::buffer(&address_type_, 1),
          boost::asio::buffer(address6_),
          boost::asio::buffer(&port_high_byte_, 1),
          boost::asio::buffer(&port_low_byte_, 1),
      }};
    }
    return {{
        boost::asio::buffer(&version_, 1),
        boost::asio::buffer(&status_, 1),
        boost::asio::buffer(&null_byte_, 1),
        boost::asio::buffer(&address_type_, 1),
        boost::asio::buffer(address4_),
        boost::asio::buffer(&port_high_byte_, 1),
        boost::asio::buffer(&port_low_byte_, 1),
    }};
  }

  bool success() const {
    return null_byte_ == 0 && status_ == request_granted &&
           (address_type_ == address_type::ipv4 ||
            address_type_ == address_type::ipv6);
  }

  uint8_t status() const { return status_; }

  uint8_t &mutable_status() { return status_; }

  boost::asio::ip::tcp::endpoint endpoint() const {
    unsigned short port = port_high_byte_;
    port = (port << 8) & 0xff00;
    port = port | port_low_byte_;

    if (address_type_ == address_type::ipv4) {
      boost::asio::ip::address_v4 address(address4_);

      return boost::asio::ip::tcp::endpoint(address, port);
    } else {
      boost::asio::ip::address_v6 address(address6_);

      return boost::asio::ip::tcp::endpoint(address, port);
    }
  }

  void set_endpoint(const boost::asio::ip::tcp::endpoint &endpoint) {
    if (endpoint.protocol() != boost::asio::ip::tcp::v4()) {
      address_type_ = ipv6;
      address6_ = endpoint.address().to_v6().to_bytes();
    } else {
      address_type_ = ipv4;
      address4_ = endpoint.address().to_v4().to_bytes();
    }

    // Convert port number to network byte order.
    unsigned short port = endpoint.port();
    port_high_byte_ = (port >> 8) & 0xff;
    port_low_byte_ = port & 0xff;
  }

public:
  static const uint8_t kHeaderLength = 4;

private:
  uint8_t version_;
  uint8_t status_;
  uint8_t null_byte_;
  uint8_t address_type_;
  boost::asio::ip::address_v4::bytes_type address4_;
  boost::asio::ip::address_v6::bytes_type address6_;
  uint8_t port_high_byte_;
  uint8_t port_low_byte_;
};

#undef PACK

} // namespace socks5

#endif // H_CORE_SOCKS5
