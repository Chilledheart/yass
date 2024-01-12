// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include <absl/flags/flag.h>
#include <gmock/gmock.h>
#include "net/cipher.hpp"
#include "core/rand_util.hpp"

#include "test_util.hpp"

using namespace net;

namespace {
std::unique_ptr<IOBuf> GenerateRandContent(int size) {
  auto buf = IOBuf::create(size);

  RandBytes(buf->mutable_data(), std::min(256, size));
  for (int i = 1; i < size / 256; ++i) {
    memcpy(buf->mutable_data() + 256 * i, buf->data(), 256);
  }
  if (size % 256) {
    memcpy(buf->mutable_data() + 256 * (size / 256),
           buf->data(), std::min(256, size % 256));
  }
  buf->append(size);

  return buf;
}
} // anonymous namespace

class CipherTest : public ::testing::TestWithParam<size_t>,
                   public cipher_visitor_interface {
 public:
  void SetUp() override {}
  void TearDown() override {}

  bool on_received_data(std::shared_ptr<IOBuf> buf) override {
    if (!recv_buf_) {
      recv_buf_ = IOBuf::create(SOCKET_DEBUF_SIZE);
    }
    recv_buf_->reserve(0, buf->length());
    memcpy(recv_buf_->mutable_tail(), buf->data(), buf->length());
    recv_buf_->append(buf->length());
    return true;
  }

  void on_protocol_error() override { ec_ = asio::error::connection_aborted; }

 protected:
  void EncodeAndDecode(const std::string& key,
                       const std::string& password,
                       cipher_method crypto_method,
                       size_t size) {
    auto encoder = std::make_unique<cipher>(key, password, crypto_method, this, true);
    auto decoder = std::make_unique<cipher>(key, password, crypto_method, this, false);
    auto send_buf = GenerateRandContent(size);
    std::shared_ptr<IOBuf> cipherbuf = IOBuf::create(size + 100);
    encoder->encrypt(send_buf->data(), send_buf->length(), cipherbuf);
    decoder->process_bytes(cipherbuf);
    ASSERT_EQ(ec_, asio::error_code());

    ASSERT_EQ(send_buf->length(), recv_buf_->length());
    ASSERT_EQ(::testing::Bytes(send_buf->data(), size),
              ::testing::Bytes(recv_buf_->data(), size));
  }

  asio::error_code ec_;
  std::shared_ptr<IOBuf> recv_buf_;
};

#define XX(num, name, string) \
  TEST_P(CipherTest, name) { \
    EncodeAndDecode("", "<dummy-password>", CRYPTO_##name, GetParam()); \
  }

CIPHER_METHOD_OLD_MAP(XX)
#undef XX

INSTANTIATE_TEST_SUITE_P(SizedCipherTest,
                         CipherTest,
                         ::testing::Values(16, 256, 512, 1024, 2048, 4096, 16 * 1024 -1),
                         ::testing::PrintToStringParamName()
);

