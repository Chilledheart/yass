// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_NET_DNS_ADDRINFO_HELPER_HPP
#define H_NET_DNS_ADDRINFO_HELPER_HPP

#include <string>
#include "net/dns_message_response.hpp"

extern "C" struct addrinfo;

namespace net {

bool is_localhost(std::string_view host);
struct addrinfo* addrinfo_loopback(bool is_ipv6, int port);
struct addrinfo* addrinfo_dup(bool is_ipv6, const net::dns_message::response& response, int port);
void addrinfo_freedup(struct addrinfo* addrinfo);

}  // namespace net

#endif  // H_NET_DNS_ADDRINFO_HELPER_HPP
