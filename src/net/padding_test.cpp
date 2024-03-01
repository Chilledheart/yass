// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#include <gtest/gtest-message.h>
#include <gtest/gtest.h>

#include <gmock/gmock.h>
#include "net/padding.hpp"

#include "test_util.hpp"

using namespace net;

TEST(NetworkTest, AddAndRemovePadding) {
  std::shared_ptr<IOBuf> buf = IOBuf::create(256);
  for (size_t i = 0; i < buf->capacity(); ++i) {
    buf->mutable_buffer()[i] = i & 255;
  }
  buf->append(256);

  std::shared_ptr<IOBuf> send_buf = IOBuf::copyBuffer(buf->data(), buf->length());

  AddPadding(send_buf);

  asio::error_code ec;
  auto recv_buf = RemovePadding(send_buf, ec);
  EXPECT_TRUE(send_buf->empty());
  EXPECT_FALSE(ec);
  EXPECT_EQ(recv_buf->length(), buf->length());

  ASSERT_EQ(::testing::Bytes(recv_buf->data(), recv_buf->length()), ::testing::Bytes(buf->data(), buf->length()));
}
