// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/dns_addrinfo_helper.hpp"

#include "net/asio.hpp"

#ifdef _WIN32
#include <ws2tcpip.h>
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0x00000008
#endif
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace net {

using namespace dns_message;

/* RFC6761 6.3 says : The domain "localhost." and any names falling within ".localhost." */
bool is_localhost(const std::string& host) {
  if (host.empty()) {
    return false;
  }
  if (host == "localhost") {
    return true;
  }
  constexpr const char suffix[] = ".localhost";
  constexpr const int suffixLength = std::size(suffix) - 1;
  static_assert(suffixLength == 10);
  if (host.size() < suffixLength) {
    return false;
  }

  return 0 == host.compare(host.size() - suffixLength, suffixLength, suffix, suffixLength);
}

// TODO more strictly we should load loopback address from system first
struct addrinfo* addrinfo_loopback(bool is_ipv6, int port) {
  struct addrinfo* addrinfo = new struct addrinfo;
  if (is_ipv6) {
    addrinfo->ai_next = nullptr;

    auto aaaa = asio::ip::address_v6::loopback();

    addrinfo->ai_flags = AI_CANONNAME | AI_NUMERICSERV;
    addrinfo->ai_family = AF_INET6;
    addrinfo->ai_socktype = SOCK_STREAM;
    addrinfo->ai_protocol = 0;
    addrinfo->ai_canonname = nullptr;

    struct sockaddr_in6* in6 = new struct sockaddr_in6;
    memset(in6, 0, sizeof(*in6));

    in6->sin6_family = AF_INET6;
    in6->sin6_port = htons(port);
    auto bytes = aaaa.to_bytes();
    static_assert(sizeof(bytes) == sizeof(in6->sin6_addr));
    memcpy(&in6->sin6_addr, &bytes, sizeof(bytes));

    addrinfo->ai_addr = (struct sockaddr*)in6;
    addrinfo->ai_addrlen = sizeof(*in6);
    addrinfo->ai_next = nullptr;
  } else {
    addrinfo->ai_next = nullptr;

    auto a = asio::ip::address_v4::loopback();

    addrinfo->ai_flags = AI_CANONNAME | AI_NUMERICSERV;
    addrinfo->ai_family = AF_INET;
    addrinfo->ai_socktype = SOCK_STREAM;
    addrinfo->ai_protocol = 0;
    addrinfo->ai_canonname = nullptr;

    struct sockaddr_in* in = new struct sockaddr_in;
    memset(in, 0, sizeof(*in));

    in->sin_family = AF_INET;
    in->sin_port = htons(port);
    auto bytes = a.to_bytes();
    static_assert(sizeof(bytes) == sizeof(in->sin_addr));
    memcpy(&in->sin_addr, &bytes, sizeof(bytes));

    addrinfo->ai_addr = (struct sockaddr*)in;
    addrinfo->ai_addrlen = sizeof(*in);
    addrinfo->ai_next = nullptr;
  }
  return addrinfo;
}

// create addrinfo
struct addrinfo* addrinfo_dup(bool is_ipv6, const net::dns_message::response& response, int port) {
  struct addrinfo* addrinfo = new struct addrinfo;
  addrinfo->ai_next = nullptr;
  // If hints.ai_flags includes the AI_CANONNAME flag, then the
  // ai_canonname field of the first of the addrinfo structures in the
  // returned list is set to point to the official name of the host.
#ifdef _WIN32
  char* canon_name = !response.cname().empty() ? _strdup(response.cname().front().c_str()) : nullptr;
#else
  char* canon_name = !response.cname().empty() ? strdup(response.cname().front().c_str()) : nullptr;
#endif

  // Iterate the output
  struct addrinfo* next_addrinfo = addrinfo;
  if (is_ipv6) {
    for (const auto& aaaa : response.aaaa()) {
      next_addrinfo->ai_next = new struct addrinfo;
      next_addrinfo = next_addrinfo->ai_next;

      next_addrinfo->ai_flags = AI_CANONNAME | AI_NUMERICSERV;
      next_addrinfo->ai_family = AF_INET6;
      next_addrinfo->ai_socktype = SOCK_STREAM;
      next_addrinfo->ai_protocol = 0;
      next_addrinfo->ai_canonname = canon_name;

      struct sockaddr_in6* in6 = new struct sockaddr_in6;
      memset(in6, 0, sizeof(*in6));

      in6->sin6_family = AF_INET6;
      in6->sin6_port = htons(port);
      auto bytes = aaaa.to_bytes();
      memcpy(&in6->sin6_addr, &bytes, sizeof(bytes));
      static_assert(sizeof(bytes) == sizeof(in6->sin6_addr));

      next_addrinfo->ai_addr = (struct sockaddr*)in6;
      next_addrinfo->ai_addrlen = sizeof(*in6);
      next_addrinfo->ai_next = nullptr;

      canon_name = nullptr;
    }
  } else {
    for (const auto& a : response.a()) {
      next_addrinfo->ai_next = new struct addrinfo;
      next_addrinfo = next_addrinfo->ai_next;

      next_addrinfo->ai_flags = AI_CANONNAME | AI_NUMERICSERV;
      next_addrinfo->ai_family = AF_INET;
      next_addrinfo->ai_socktype = SOCK_STREAM;
      next_addrinfo->ai_protocol = 0;
      next_addrinfo->ai_canonname = (char*)canon_name;

      struct sockaddr_in* in = new struct sockaddr_in;
      memset(in, 0, sizeof(*in));

      in->sin_family = AF_INET;
      in->sin_port = htons(port);
      auto bytes = a.to_bytes();
      memcpy(&in->sin_addr, &bytes, sizeof(bytes));
      static_assert(sizeof(bytes) == sizeof(in->sin_addr));

      next_addrinfo->ai_addr = (struct sockaddr*)in;
      next_addrinfo->ai_addrlen = sizeof(*in);
      next_addrinfo->ai_next = nullptr;

      canon_name = nullptr;
    }
  }
  if (canon_name) {
    free(canon_name);
  }

  struct addrinfo* prev_addrinfo = addrinfo;
  addrinfo = addrinfo->ai_next;
  delete prev_addrinfo;
  return addrinfo;
}

// free addrinfo
void addrinfo_freedup(struct addrinfo* addrinfo) {
  struct addrinfo* next_addrinfo;
  while (addrinfo) {
    if (addrinfo->ai_family == AF_INET6) {
      struct sockaddr_in6* in6 = (struct sockaddr_in6*)addrinfo->ai_addr;
      delete in6;
    } else {
      struct sockaddr_in* in = (struct sockaddr_in*)addrinfo->ai_addr;
      delete in;
    }
    if (addrinfo->ai_canonname) {
      free(addrinfo->ai_canonname);
    }
    next_addrinfo = addrinfo->ai_next;
    delete addrinfo;
    addrinfo = next_addrinfo;
  }
}

}  // namespace net
