// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include <gtest/gtest.h>

#include "net/dns_addrinfo_helper.hpp"

using namespace net;

TEST(DnsAddrInfoHelper, IsLocalhost) {
  EXPECT_TRUE(is_localhost("localhost"));
  EXPECT_FALSE(is_localhost("badlocalhost"));
  EXPECT_FALSE(is_localhost("localhostbad"));
  EXPECT_TRUE(is_localhost(".localhost"));
  EXPECT_TRUE(is_localhost("good.localhost"));
  EXPECT_FALSE(is_localhost(".localhostbad"));
  EXPECT_FALSE(is_localhost(".badlocalhost"));
  EXPECT_FALSE(is_localhost(".bad"));
}

TEST(DnsAddrInfoHelper, LoopbackIpv4) {
  struct addrinfo* addr = addrinfo_loopback(false, 80);
  EXPECT_EQ(AF_INET, addr->ai_family);
  ASSERT_EQ(sizeof(struct sockaddr_in), (size_t)addr->ai_addrlen);
  EXPECT_EQ(nullptr, addr->ai_canonname);
  EXPECT_EQ(nullptr, addr->ai_next);
  struct sockaddr_in* in = (struct sockaddr_in*)addr->ai_addr;
  EXPECT_EQ(AF_INET, in->sin_family);
  EXPECT_EQ(htons(80), in->sin_port);
  asio::ip::address_v4::bytes_type addrv;
  static_assert(sizeof(struct in_addr) == sizeof(asio::ip::address_v4::bytes_type));
  memcpy(&addrv, &in->sin_addr, sizeof(asio::ip::address_v4::bytes_type));
  EXPECT_TRUE(asio::ip::make_address_v4(addrv).is_loopback());
  addrinfo_freedup(addr);
}

TEST(DnsAddrInfoHelper, LoopbackIpv6) {
  struct addrinfo* addr = addrinfo_loopback(true, 80);
  EXPECT_EQ(AF_INET6, addr->ai_family);
  ASSERT_EQ(sizeof(struct sockaddr_in6), (size_t)addr->ai_addrlen);
  EXPECT_EQ(nullptr, addr->ai_canonname);
  EXPECT_EQ(nullptr, addr->ai_next);
  struct sockaddr_in6* in6 = (struct sockaddr_in6*)addr->ai_addr;
  EXPECT_EQ(AF_INET6, in6->sin6_family);
  EXPECT_EQ(htons(80), in6->sin6_port);
  asio::ip::address_v6::bytes_type addrv6;
  static_assert(sizeof(struct in6_addr) == sizeof(asio::ip::address_v6::bytes_type));
  memcpy(&addrv6, &in6->sin6_addr, sizeof(asio::ip::address_v6::bytes_type));
  EXPECT_TRUE(asio::ip::make_address_v6(addrv6).is_loopback());
  addrinfo_freedup(addr);
}
