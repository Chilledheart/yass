// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include <absl/flags/flag.h>
#include <gmock/gmock.h>
#include "core/cipher.hpp"
#include "core/rand_util.hpp"

#include "test_util.hpp"

namespace {
std::unique_ptr<IOBuf> GenerateRandContent(int size) {
  auto buf = IOBuf::create(size);
  RandBytes(buf->mutable_data(), std::min(256, size));
  for (int i = 1; i < size / 256; ++i) {
    memcpy(buf->mutable_data() + 256 * i, buf->data(), 256);
  }
  buf->append(size);
  return buf;
}
} // anonymous namespace

class CipherTest : public ::testing::TestWithParam<size_t> {
 public:
  void SetUp() override {}
  void TearDown() override {}

 protected:
  void EncodeAndDecode(const std::string& key,
                       const std::string& password,
                       cipher_method crypto_method,
                       size_t size) {
    auto encoder = std::make_unique<cipher>(key, password, crypto_method, true);
    auto decoder = std::make_unique<cipher>(key, password, crypto_method, false);
    auto send_buf = GenerateRandContent(size);
    std::shared_ptr<IOBuf> cipherbuf = IOBuf::create(size + 100);
    encoder->encrypt(send_buf.get(), &cipherbuf);
    std::shared_ptr<IOBuf> recv_buf = IOBuf::create(size);
    decoder->decrypt(cipherbuf.get(), &recv_buf);

    ASSERT_EQ(send_buf->length(), recv_buf->length());
    ASSERT_EQ(::testing::Bytes(send_buf->data(), size),
              ::testing::Bytes(recv_buf->data(), size));
  }
};

#define XX(num, name, string) \
  TEST_P(CipherTest, name) { \
    EncodeAndDecode("", "<dummy-password>", CRYPTO_##name, GetParam()); \
  }

CIPHER_METHOD_VALID_MAP(XX)
#undef XX

INSTANTIATE_TEST_SUITE_P(SizedCipherTest,
                         CipherTest,
                         ::testing::Values(16, 256, 512, 1024, 2048, 4096, 16 * 1024 -1),
                         ::testing::PrintToStringParamName()
);

