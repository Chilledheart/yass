// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include <absl/flags/flag.h>
#include <build/build_config.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest.h>

#include <gmock/gmock.h>

#include "net/doh_resolver.hpp"
#include "test_util.hpp"

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
ABSL_FLAG(bool, no_doh_tests, true, "skip doh tests");
#else
ABSL_FLAG(bool, no_doh_tests, false, "skip doh tests");
#endif

using namespace net;

static void DoRemoteResolve(asio::io_context& io_context, scoped_refptr<DoHResolver> resolver) {
  auto work_guard =
      std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  io_context.restart();

  asio::post(io_context, [&]() {
    resolver->AsyncResolve("www.google.com", 80,
                           [&](asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
                             work_guard.reset();
                             ASSERT_FALSE(ec) << ec;
                             for (auto iter = std::begin(results); iter != std::end(results); ++iter) {
                               const asio::ip::tcp::endpoint& endpoint = *iter;
                               auto addr = endpoint.address();
                               EXPECT_FALSE(addr.is_loopback()) << addr;
                               EXPECT_FALSE(addr.is_unspecified()) << addr;
                             }
                           });
  });

  EXPECT_NO_FATAL_FAILURE(io_context.run());
}

TEST(DOH_TEST, RemoteBasic) {
  if (absl::GetFlag(FLAGS_no_doh_tests)) {
    GTEST_SKIP() << "skipped as required";
    return;
  }
  asio::error_code ec;
  asio::io_context io_context;

  auto resolver = DoHResolver::Create(io_context);
  int ret = resolver->Init("https://1.1.1.1/dns-query", 5000);
  ASSERT_EQ(ret, 0);

  DoRemoteResolve(io_context, resolver);
}

TEST(DOH_TEST, RemoteMulti) {
  if (absl::GetFlag(FLAGS_no_doh_tests)) {
    GTEST_SKIP() << "skipped as required";
    return;
  }
  asio::error_code ec;
  asio::io_context io_context;

  auto resolver = DoHResolver::Create(io_context);
  int ret = resolver->Init("https://1.1.1.1/dns-query", 5000);
  ASSERT_EQ(ret, 0);

  DoRemoteResolve(io_context, resolver);
  DoRemoteResolve(io_context, resolver);
  DoRemoteResolve(io_context, resolver);
  DoRemoteResolve(io_context, resolver);
  DoRemoteResolve(io_context, resolver);
}

TEST(DOH_TEST, Timeout) {
  if (absl::GetFlag(FLAGS_no_doh_tests)) {
    GTEST_SKIP() << "skipped as required";
    return;
  }
  asio::error_code ec;
  asio::io_context io_context;

  auto resolver = DoHResolver::Create(io_context);
  int ret = resolver->Init("https://2.2.2.2/dns-query", 1);
  ASSERT_EQ(ret, 0);

  auto work_guard =
      std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  asio::post(io_context, [&]() {
    resolver->AsyncResolve("www.google.com", 80,
                           [&](asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
                             work_guard.reset();
                             ASSERT_EQ(ec, asio::error::timed_out) << ec;
                           });
  });

  EXPECT_NO_FATAL_FAILURE(io_context.run());
}
