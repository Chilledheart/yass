// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifdef HAVE_C_ARES

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>
#include <absl/flags/flag.h>
#include <build/build_config.h>

#include <gmock/gmock.h>

#include "test_util.hpp"
#include "core/c-ares.hpp"

#ifdef OS_ANDROID
ABSL_FLAG(bool, no_cares_tests, true, "skip c-ares tests");
#else
ABSL_FLAG(bool, no_cares_tests, false, "skip c-ares tests");
#endif

TEST(CARES_TEST, LocalfileBasic) {
  asio::error_code ec;
  asio::io_context io_context;
  auto resolver = CAresResolver::Create(io_context);
  int ret = resolver->Init(10);
  ASSERT_EQ(ret, 0);
  auto work_guard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  asio::post(io_context, [&]() {
    resolver->AsyncResolve("localhost", "80",
      [&](asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
        work_guard.reset();
        ASSERT_FALSE(ec) << ec;
        for (auto iter = std::begin(results); iter != std::end(results); ++iter) {
          asio::ip::tcp::endpoint endpoint = *iter;
          auto addr = endpoint.address();
          EXPECT_TRUE(addr.is_loopback()) << addr;
        }
    });
  });

  io_context.run();
}

TEST(CARES_TEST, RemoteNotFound) {
  if (absl::GetFlag(FLAGS_no_cares_tests)) {
    GTEST_SKIP() << "skipped as required";
    return;
  }
  asio::error_code ec;
  asio::io_context io_context;

  auto resolver = CAresResolver::Create(io_context);
  int ret = resolver->Init(10);
  ASSERT_EQ(ret, 0);

  auto work_guard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  asio::post(io_context, [&]() {
    resolver->AsyncResolve("not-found.invalid", "80",
      [&](asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
        work_guard.reset();
        ASSERT_TRUE(ec) << ec;
    });
  });

  io_context.run();
}

static void DoRemoteResolve(asio::io_context& io_context, scoped_refptr<CAresResolver> resolver) {
  auto work_guard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  io_context.restart();

  asio::post(io_context, [&]() {
    resolver->AsyncResolve("www.google.com", "80",
      [&](asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
        work_guard.reset();
        // Sometimes c-ares don't get ack in time, ignore it safely
        if (ec == asio::error::timed_out) {
          return;
        }
        ASSERT_FALSE(ec) << ec;
        for (auto iter = std::begin(results); iter != std::end(results); ++iter) {
          const asio::ip::tcp::endpoint &endpoint = *iter;
          auto addr = endpoint.address();
          EXPECT_FALSE(addr.is_loopback()) << addr;
          EXPECT_FALSE(addr.is_unspecified()) << addr;
        }
    });
  });

  EXPECT_NO_FATAL_FAILURE(io_context.run());
}

TEST(CARES_TEST, RemoteBasic) {
  if (absl::GetFlag(FLAGS_no_cares_tests)) {
    GTEST_SKIP() << "skipped as required";
    return;
  }
  asio::error_code ec;
  asio::io_context io_context;

  auto resolver = CAresResolver::Create(io_context);
  int ret = resolver->Init(5000);
  ASSERT_EQ(ret, 0);

  DoRemoteResolve(io_context, resolver);
}

TEST(CARES_TEST, RemoteMulti) {
  if (absl::GetFlag(FLAGS_no_cares_tests)) {
    GTEST_SKIP() << "skipped as required";
    return;
  }
  asio::error_code ec;
  asio::io_context io_context;

  auto resolver = CAresResolver::Create(io_context);
  int ret = resolver->Init(5000);
  ASSERT_EQ(ret, 0);

  DoRemoteResolve(io_context, resolver);
  DoRemoteResolve(io_context, resolver);
  DoRemoteResolve(io_context, resolver);
  DoRemoteResolve(io_context, resolver);
  DoRemoteResolve(io_context, resolver);
}

#endif // HAVE_C_ARES
