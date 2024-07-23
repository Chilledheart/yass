// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include <gtest/gtest-message.h>
#include <gtest/gtest.h>

#include <gmock/gmock.h>

#include "core/span.hpp"
#include "net/dns_message_request.hpp"
#include "net/dns_message_response_parser.hpp"
#include "net/iobuf.hpp"

#include "test_util.hpp"

#ifdef HAVE_C_ARES
#include <ares.h>
#include <set>
#endif

using namespace net;
using namespace net::dns_message;

#ifdef HAVE_C_ARES
static span<const uint8_t> CreateRequery(const char* name, int dnsclass, int type) {
  uint8_t* buf = nullptr;
  int buflen = 0;
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  // defined(__clang__)
  EXPECT_EQ(ARES_SUCCESS, ares_create_query(name, dnsclass, type, 0, 0x1, &buf, &buflen, 0));
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif  // defined(__clang__)
  static uint8_t poolbuf[1500];
  if (buflen == 0 || buflen > (int)sizeof(poolbuf)) {
    return {};
  }
  memcpy(poolbuf, buf, buflen);
  ares_free_string(buf);
  return span<const uint8_t>(poolbuf, static_cast<size_t>(buflen));
}
#endif

TEST(DnsMessageTest, Request) {
  request msg;
  ASSERT_TRUE(msg.init("www.google.com", DNS_TYPE_AAAA));
  ASSERT_TRUE(msg.init("www.google.com.", DNS_TYPE_AAAA));
  ASSERT_FALSE(msg.init("www.google..com", DNS_TYPE_AAAA));
  ASSERT_FALSE(msg.init("www.google-long-long-long-long-long-long-long-long-long-long-long-long.com", DNS_TYPE_AAAA));
}

#ifdef HAVE_C_ARES
TEST(DnsMessageTest, RequestCAresMatch) {
  request msg;
  ASSERT_TRUE(msg.init("www.google.com", DNS_TYPE_AAAA));

  auto source = IOBuf::create(SOCKET_BUF_SIZE);

  for (auto buffer : msg.buffers()) {
    source->reserve(0, buffer.size());
    memcpy(source->mutable_tail(), buffer.data(), buffer.size());
    source->append(buffer.size());
  }

  auto target = CreateRequery("www.google.com", DNS_CLASS_IN, DNS_TYPE_AAAA);

  ASSERT_EQ(::testing::Bytes(*source), ::testing::Bytes(target));
}
#endif

static constexpr const uint8_t kExpectedRequestBytes[] = {
    0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x77, 0x77, 0x77,
    0x05, 0x62, 0x61, 0x69, 0x64, 0x75, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01};

TEST(DnsMessageTest, RequestBytes) {
  request msg;
  ASSERT_TRUE(msg.init("www.baidu.com", DNS_TYPE_A));
  IOBuf buf;
  for (auto buffer : msg.buffers()) {
    buf.reserve(0, buffer.size());
    memcpy(buf.mutable_tail(), buffer.data(), buffer.size());
    buf.append(buffer.size());
  }

  ASSERT_EQ(::testing::Bytes(buf.data(), buf.length()),
            ::testing::Bytes(kExpectedRequestBytes, std::size(kExpectedRequestBytes)));
}

static constexpr const uint8_t kResponseBytes[] = {
    0x9e, 0x98, 0x81, 0x80, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x03, 0x77, 0x77, 0x77, 0x05, 0x62, 0x61,
    0x69, 0x64, 0x75, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x05, 0x00, 0x01, 0x00,
    0x00, 0x04, 0xab, 0x00, 0x0f, 0x03, 0x77, 0x77, 0x77, 0x01, 0x61, 0x06, 0x73, 0x68, 0x69, 0x66, 0x65, 0x6e, 0xc0,
    0x16, 0xc0, 0x2b, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x19, 0x00, 0x0e, 0x03, 0x77, 0x77, 0x77, 0x07, 0x77,
    0x73, 0x68, 0x69, 0x66, 0x65, 0x6e, 0xc0, 0x16, 0xc0, 0x46, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x27, 0x00,
    0x04, 0x67, 0xeb, 0x2f, 0x67, 0xc0, 0x46, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x27, 0x00, 0x04, 0x67, 0xeb,
    0x2e, 0x28, 0x00, 0x00, 0x29, 0x04, 0xd0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

TEST(DnsMessageTest, AAndCnameResponseBytes) {
  /// parser of handshake request
  response_parser response_parser;
  /// copy of handshake response
  response response;

  response_parser::result_type result;
  std::tie(result, std::ignore) =
      response_parser.parse(response, std::begin(kResponseBytes), std::begin(kResponseBytes), std::end(kResponseBytes));
  ASSERT_EQ(result, response_parser::good);
  ASSERT_EQ(0x9e98, response.id());
  std::vector<asio::ip::address_v4> expected_addr{asio::ip::make_address("103.235.47.103").to_v4(),
                                                  asio::ip::make_address("103.235.46.40").to_v4()};
  ASSERT_EQ(expected_addr, response.a());
  std::vector<std::string> expected_cname{
      "www.a.shifen.com",
      "www.wshifen.com",
  };
  ASSERT_EQ(expected_cname, response.cname());
}

#ifdef HAVE_C_ARES
TEST(DnsMessageTest, AAndCnameResponseBytesCAresMatch) {
  struct hostent* host = nullptr;
  struct ares_addrttl info[2] = {};
  int count = std::size(info);
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  // defined(__clang__)
  ASSERT_EQ(ARES_SUCCESS, ares_parse_a_reply(kResponseBytes, std::size(kResponseBytes), &host, info, &count));
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif  // defined(__clang__)
  ASSERT_EQ(2, count);
  ASSERT_EQ(25, info[0].ttl);
  ASSERT_EQ(25, info[1].ttl);

  std::set<asio::ip::address_v4> info_addr;
  for (int i = 0; i < count; ++i) {
    info_addr.emplace(ntohl(info[i].ipaddr.s_addr));
  }
  std::set<asio::ip::address_v4> expected_addr{asio::ip::make_address("103.235.47.103").to_v4(),
                                               asio::ip::make_address("103.235.46.40").to_v4()};
  ASSERT_EQ(expected_addr, info_addr);

  ASSERT_NE(nullptr, host);
  ASSERT_EQ(AF_INET, host->h_addrtype);
  std::set<std::string> cname;
  cname.insert(host->h_name);
  for (int i = 0; host->h_aliases[i]; ++i) {
    cname.insert(host->h_aliases[i]);
  }
  std::set<std::string> expected_cname{
      "www.baidu.com",
      "www.a.shifen.com",
  };
  ASSERT_EQ(expected_cname, cname);

  std::set<asio::ip::address_v4> host_addr;
  for (int i = 0; host->h_addr_list[i]; ++i) {
    host_addr.emplace(ntohl(((struct in_addr*)(host->h_addr_list[i]))->s_addr));
  }
  ASSERT_EQ(expected_addr, host_addr);
  ares_free_hostent(host);
}
#endif
