// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifdef HAVE_C_ARES

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include <gmock/gmock.h>

#include "test_util.hpp"
#include "core/c-ares.hpp"

TEST(CARES_TEST, LocalfileBasic) {
  asio::error_code ec;
  asio::io_context io_context;
  auto work_guard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  asio::post(io_context, [&]() {
    auto resolver = CAresResolver::Create(io_context);
    int ret = resolver->Init(1000);
    if (ret) {
      work_guard.reset();
    }
    ASSERT_EQ(ret, 0);
    resolver->AsyncResolve("localhost", "80", [&](
      asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
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
  asio::error_code ec;
  asio::io_context io_context;
  auto work_guard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  asio::post(io_context, [&]() {
    auto resolver = CAresResolver::Create(io_context);
    int ret = resolver->Init(10);
    if (ret) {
      work_guard.reset();
    }
    ASSERT_EQ(ret, 0);
    resolver->AsyncResolve("not-found.invalid", "80", [&](
      asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
        work_guard.reset();
        ASSERT_TRUE(ec) << ec;
    });
  });

  io_context.run();
}

TEST(CARES_TEST, RemoteBasic) {
  asio::error_code ec;
  asio::io_context io_context;
  auto work_guard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  asio::post(io_context, [&]() {
    auto resolver = CAresResolver::Create(io_context);
    int ret = resolver->Init(5000);
    if (ret) {
      work_guard.reset();
    }
    ASSERT_EQ(ret, 0);
    resolver->AsyncResolve("www.apple.com", "80", [&](
      asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
        work_guard.reset();
        ASSERT_FALSE(ec) << ec;
        for (auto iter = std::begin(results); iter != std::end(results); ++iter) {
          asio::ip::tcp::endpoint endpoint = *iter;
          auto addr = endpoint.address();
          EXPECT_FALSE(addr.is_loopback()) << addr;
          EXPECT_FALSE(addr.is_unspecified()) << addr;
        }
    });
  });

  io_context.run();
}

#endif // HAVE_C_ARES
