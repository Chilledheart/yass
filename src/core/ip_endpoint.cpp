//
// ip_endpoint.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "core/ip_endpoint.hpp"

#include "core/logging.hpp"
#include "core/pr_util.hpp"

// By definition, int is large enough to hold both sizes.
static const int kSockaddrInSize = sizeof(struct sockaddr_in);
static const int kSockaddrIn6Size = sizeof(struct sockaddr_in6);

// Extracts the address and port portions of a sockaddr.
static bool GetIPAddressFromSockAddr(const struct sockaddr *sock_addr,
                                     int sock_addr_len, const uint8_t **address,
                                     size_t *address_len, uint16_t *port) {
  if (sock_addr->sa_family == AF_INET) {
    if (sock_addr_len < static_cast<int>(sizeof(struct sockaddr_in)))
      return false;
    const struct sockaddr_in *addr =
        reinterpret_cast<const struct sockaddr_in *>(sock_addr);
    *address = reinterpret_cast<const uint8_t *>(&addr->sin_addr);
    *address_len = IPAddress::kIPv4AddressSize;
    if (port)
      *port = ntohs(addr->sin_port);
    return true;
  }

  if (sock_addr->sa_family == AF_INET6) {
    if (sock_addr_len < static_cast<int>(sizeof(struct sockaddr_in6)))
      return false;
    const struct sockaddr_in6 *addr =
        reinterpret_cast<const struct sockaddr_in6 *>(sock_addr);
    *address = reinterpret_cast<const uint8_t *>(&addr->sin6_addr);
    *address_len = IPAddress::kIPv6AddressSize;
    if (port)
      *port = ntohs(addr->sin6_port);
    return true;
  }

  return false; // Unrecognized |sa_family|.
}

IPEndPoint::IPEndPoint() : port_(0) {}

IPEndPoint::~IPEndPoint() = default;

IPEndPoint::IPEndPoint(const IPAddress &address, uint16_t port)
    : address_(address), port_(port) {}

IPEndPoint::IPEndPoint(const IPEndPoint &endpoint) {
  address_ = endpoint.address_;
  port_ = endpoint.port_;
}

AddressFamily IPEndPoint::GetFamily() const {
  return GetAddressFamily(address_);
}

int IPEndPoint::GetSockAddrFamily() const {
  switch (address_.size()) {
  case IPAddress::kIPv4AddressSize:
    return AF_INET;
  case IPAddress::kIPv6AddressSize:
    return AF_INET6;
  default:
    NOTREACHED() << "Bad IP address";
    return AF_UNSPEC;
  }
}

bool IPEndPoint::ToSockAddr(struct sockaddr *address,
                            int *address_length) const {
  DCHECK(address);
  DCHECK(address_length);
  switch (address_.size()) {
  case IPAddress::kIPv4AddressSize: {
    if (*address_length < kSockaddrInSize)
      return false;
    *address_length = kSockaddrInSize;
    struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in *>(address);
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port_);
    memcpy(&addr->sin_addr, address_.bytes().data(),
           IPAddress::kIPv4AddressSize);
    break;
  }
  case IPAddress::kIPv6AddressSize: {
    if (*address_length < kSockaddrIn6Size)
      return false;
    *address_length = kSockaddrIn6Size;
    struct sockaddr_in6 *addr6 =
        reinterpret_cast<struct sockaddr_in6 *>(address);
    memset(addr6, 0, sizeof(struct sockaddr_in6));
    addr6->sin6_family = AF_INET6;
    addr6->sin6_port = htons(port_);
    memcpy(&addr6->sin6_addr, address_.bytes().data(),
           IPAddress::kIPv6AddressSize);
    break;
  }
  default:
    return false;
  }
  return true;
}

bool IPEndPoint::FromSockAddr(const struct sockaddr *sock_addr,
                              int sock_addr_len) {
  DCHECK(sock_addr);

  const uint8_t *address;
  size_t address_len;
  uint16_t port;
  if (!GetIPAddressFromSockAddr(sock_addr, sock_addr_len, &address,
                                &address_len, &port)) {
    return false;
  }

  address_ = IPAddress(address, address_len);
  port_ = port;
  return true;
}

std::string IPEndPoint::ToString() const {
  return IPAddressToStringWithPort(address_, port_);
}

std::string IPEndPoint::ToStringWithoutPort() const {
  return address_.ToString();
}

bool IPEndPoint::operator<(const IPEndPoint &other) const {
  // Sort IPv4 before IPv6.
  if (address_.size() != other.address_.size()) {
    return address_.size() < other.address_.size();
  }
  return std::tie(address_, port_) < std::tie(other.address_, other.port_);
}

bool IPEndPoint::operator==(const IPEndPoint &other) const {
  return address_ == other.address_ && port_ == other.port_;
}

bool IPEndPoint::operator!=(const IPEndPoint &that) const {
  return !(*this == that);
}
