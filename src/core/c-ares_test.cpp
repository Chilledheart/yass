// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include <gmock/gmock.h>

#include "test_util.hpp"
#include "core/c-ares.hpp"

TEST(CARES_TEST, LocalfileBasic) {
  asio::error_code ec;
  asio::io_context io_context;
  auto work_guard = std::make_unique<asio::io_context::work>(io_context);

  io_context.post([&]() {
    auto resolver = CAresResolver::Create(io_context);
    int ret = resolver->Init(3000, 1);
    if (ret) {
      work_guard.reset();
    }
    ASSERT_EQ(ret, 0);
    resolver->AsyncResolve("localhost", "80", [&](
      asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
        if (ec) {
          work_guard.reset();
        }
        ASSERT_FALSE(ec) << ec;
        for (auto result : results) {
          auto addr = result.endpoint().address();
          EXPECT_TRUE(addr.is_loopback()) << addr;
        }

        work_guard.reset();
    });
  });

  io_context.run(ec);
}

TEST(CARES_TEST, RemoteBasic) {
  asio::error_code ec;
  asio::io_context io_context;
  auto work_guard = std::make_unique<asio::io_context::work>(io_context);

  io_context.post([&]() {
    auto resolver = CAresResolver::Create(io_context);
    int ret = resolver->Init(3000, 1);
    if (ret) {
      work_guard.reset();
    }
    ASSERT_EQ(ret, 0);
    resolver->AsyncResolve("www.baidu.com", "80", [&](
      asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
        if (ec) {
          work_guard.reset();
        }
        ASSERT_FALSE(ec) << ec;
        for (auto result : results) {
          auto addr = result.endpoint().address();
          EXPECT_FALSE(addr.is_loopback()) << addr;
          EXPECT_FALSE(addr.is_unspecified()) << addr;
        }

        work_guard.reset();
    });
  });

  io_context.run(ec);
}
