// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include <absl/flags/flag.h>
#include <gmock/gmock.h>
#include "config/config_impl.hpp"
#include <cstdlib>

ABSL_FLAG(bool, test_bool, true, "Test bool value");
ABSL_FLAG(int32_t, test_signed_val, 0, "Test int32_t value");
ABSL_FLAG(uint32_t, test_unsigned_val, 0, "Test uint32_t value");
ABSL_FLAG(int64_t, test_signed_64val, 0, "Test int64_t value");
ABSL_FLAG(uint64_t, test_unsigned_64val, 0, "Test uint64_t value");
ABSL_FLAG(std::string, test_string, "", "Test string value");


#if !(defined(_WIN32) || (defined(__APPLE__) && defined(__clang__)))
ABSL_DECLARE_FLAG(std::string, configfile);
#endif

TEST(ConfigTest, ReadAfterWrite) {
#if !(defined(_WIN32) || (defined(__APPLE__) && defined(__clang__)))
  std::string original_configfile = absl::GetFlag(FLAGS_configfile);
  const char* tmpdir = getenv("TMPDIR");
  if (!tmpdir || *tmpdir == '\0')
    tmpdir = "/tmp";
  std::string tmpdir_configfile = std::string(tmpdir) + "/" + "yass.json";
  absl::SetFlag(&FLAGS_configfile, tmpdir_configfile);
#endif

  auto config_impl = config::ConfigImpl::Create();
  bool test_bool = true;
  int32_t test_signed_val = -12345;
  uint32_t test_unsigned_val = 12345U;
  int64_t test_signed_64val = -123456LL - INT32_MAX;
  uint64_t test_unsigned_64val = 123456ULL + UINT32_MAX;
  std::string test_string = "test-str";

  absl::SetFlag(&FLAGS_test_bool, test_bool);
  absl::SetFlag(&FLAGS_test_signed_val, test_signed_val);
  absl::SetFlag(&FLAGS_test_unsigned_val, test_unsigned_val);
  absl::SetFlag(&FLAGS_test_signed_64val, test_signed_64val);
  absl::SetFlag(&FLAGS_test_unsigned_64val, test_unsigned_64val);
  absl::SetFlag(&FLAGS_test_string, test_string);

  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Write("test_bool", FLAGS_test_bool));
  EXPECT_TRUE(config_impl->Write("test_signed_val", FLAGS_test_signed_val));
  EXPECT_TRUE(config_impl->Write("test_unsigned_val", FLAGS_test_unsigned_val));
  EXPECT_TRUE(config_impl->Write("test_signed_64val", FLAGS_test_signed_64val));
  EXPECT_TRUE(config_impl->Write("test_unsigned_64val", FLAGS_test_unsigned_64val));
  EXPECT_TRUE(config_impl->Write("test_string", FLAGS_test_string));
  ASSERT_TRUE(config_impl->Close());

  absl::SetFlag(&FLAGS_test_bool, false);
  absl::SetFlag(&FLAGS_test_signed_val, 0);
  absl::SetFlag(&FLAGS_test_unsigned_val, 0);
  absl::SetFlag(&FLAGS_test_signed_64val, 0);
  absl::SetFlag(&FLAGS_test_unsigned_64val, 0);
  absl::SetFlag(&FLAGS_test_string, "");

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_TRUE(config_impl->Read("test_bool", &FLAGS_test_bool));
  EXPECT_TRUE(config_impl->Read("test_signed_val", &FLAGS_test_signed_val));
  EXPECT_TRUE(config_impl->Read("test_unsigned_val", &FLAGS_test_unsigned_val));
  EXPECT_TRUE(config_impl->Read("test_signed_64val", &FLAGS_test_signed_64val));
  EXPECT_TRUE(config_impl->Read("test_unsigned_64val", &FLAGS_test_unsigned_64val));
  EXPECT_TRUE(config_impl->Read("test_string", &FLAGS_test_string));
  ASSERT_TRUE(config_impl->Close());

  EXPECT_EQ(absl::GetFlag(FLAGS_test_bool), test_bool);
  EXPECT_EQ(absl::GetFlag(FLAGS_test_signed_val), test_signed_val);
  EXPECT_EQ(absl::GetFlag(FLAGS_test_unsigned_val), test_unsigned_val);
  EXPECT_EQ(absl::GetFlag(FLAGS_test_signed_64val), test_signed_64val);
  EXPECT_EQ(absl::GetFlag(FLAGS_test_unsigned_64val), test_unsigned_64val);
  EXPECT_EQ(absl::GetFlag(FLAGS_test_string), test_string);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Delete("test_bool"));
  EXPECT_TRUE(config_impl->Delete("test_signed_val"));
  EXPECT_TRUE(config_impl->Delete("test_unsigned_val"));
  EXPECT_TRUE(config_impl->Delete("test_signed_64val"));
  EXPECT_TRUE(config_impl->Delete("test_unsigned_64val"));
  EXPECT_TRUE(config_impl->Delete("test_string"));
  ASSERT_TRUE(config_impl->Close());

#if !(defined(_WIN32) || (defined(__APPLE__) && defined(__clang__)))
  absl::SetFlag(&FLAGS_configfile, original_configfile);
#endif
}
