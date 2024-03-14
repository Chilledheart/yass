// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_NET_DNS_MESSAGE_RESPONSE
#define H_NET_DNS_MESSAGE_RESPONSE

#include "core/compiler_specific.hpp"
#include "core/logging.hpp"
#include "net/dns_message.hpp"
#include "net/protocol.hpp"

#include <stdint.h>
#include <string>
#include <vector>

namespace net {

namespace dns_message {
// Resource record format
//                                    1  1  1  1  1  1
//      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//    |                                               |
//    /                                               /
//    /                      NAME                     /
//    |                                               |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//    |                      TYPE                     |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//    |                     CLASS                     |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//    |                      TTL                      |
//    |                                               |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//    |                   RDLENGTH                    |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
//    /                     RDATA                     /
//    /                                               /
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

class response {
 public:
  response() : header_() {}

  uint16_t id() const {
    auto doh = reinterpret_cast<const uint8_t*>(&header_.id);
    return (unsigned short)((doh[0] << 8) | doh[1]);
  }

  const std::vector<asio::ip::address_v4>& a() const { return a_; }
  const std::vector<asio::ip::address_v6>& aaaa() const { return aaaa_; }
  const std::vector<std::string>& cname() const { return cname_; }

 private:
  friend class response_parser;
  header header_;
  std::vector<asio::ip::address_v4> a_;
  std::vector<asio::ip::address_v6> aaaa_;
  std::vector<std::string> cname_;
};  // dns_message

}  // namespace dns_message

}  // namespace net

#endif  // H_NET_DNS_MESSAGE_RESPONSE
