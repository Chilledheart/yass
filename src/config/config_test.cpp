// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest-message.h>
#include <gtest/gtest.h>

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <gmock/gmock.h>
#include <cstdlib>
#include "config/config_impl.hpp"
#include "core/process_utils.hpp"
#include "core/rand_util.hpp"
#include "core/utils_fs.hpp"

using namespace yass;

ABSL_FLAG(bool, test_bool, true, "Test bool value");
ABSL_FLAG(int32_t, test_signed_val, 0, "Test int32_t value");
ABSL_FLAG(uint32_t, test_unsigned_val, 0, "Test uint32_t value");
ABSL_FLAG(int64_t, test_signed_64val, 0, "Test int64_t value");
ABSL_FLAG(uint64_t, test_unsigned_64val, 0, "Test uint64_t value");
ABSL_FLAG(std::string, test_string, "", "Test string value");

class ConfigTest : public ::testing::Test {
 public:
  void SetUp() override {
    key_prefix_ = absl::StrCat("pid_", GetPID(), "_run_", RandUint64());
#if !(defined(_WIN32) || defined(__APPLE__))
    original_configfile_ = config::g_configfile;
    const char* tmpdir = getenv("TMPDIR");
    if (!tmpdir || *tmpdir == '\0')
#if defined(__ANDROID__) || defined(__OHOS__)
      tmpdir = "/data/local/tmp";
#else
      tmpdir = "/tmp";
#endif
    config::g_configfile = absl::StrCat(tmpdir, "/", "yass_unittest_", key_prefix(), ".tmp");
#endif
  }
  void TearDown() override {
#if !(defined(_WIN32) || defined(__APPLE__))
    RemoveFile(config::g_configfile);
    config::g_configfile = original_configfile_;
#endif
  }

  const std::string& key_prefix() const { return key_prefix_; }

 private:
  std::string key_prefix_;
#if !(defined(_WIN32) || defined(__APPLE__))
  std::string original_configfile_;
#endif
};

TEST_F(ConfigTest, RWBool) {
  auto config_impl = config::ConfigImpl::Create();
  constexpr bool test_bool = true;
  const std::string test_key = absl::StrCat("test_bool_", key_prefix());

  absl::SetFlag(&FLAGS_test_bool, test_bool);

  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Write(test_key, FLAGS_test_bool));
  ASSERT_TRUE(config_impl->Close());

  absl::SetFlag(&FLAGS_test_bool, false);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_TRUE(config_impl->HasKey<bool>(test_key));
  EXPECT_FALSE(config_impl->HasKey<std::string>(test_key));
  EXPECT_TRUE(config_impl->Read(test_key, &FLAGS_test_bool));
  ASSERT_TRUE(config_impl->Close());

  EXPECT_EQ(absl::GetFlag(FLAGS_test_bool), test_bool);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Delete(test_key));
  ASSERT_TRUE(config_impl->Close());

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_FALSE(config_impl->HasKey<bool>(test_key));
  ASSERT_TRUE(config_impl->Close());
}

TEST_F(ConfigTest, RWInt32) {
  auto config_impl = config::ConfigImpl::Create();
  constexpr int32_t test_signed_val = -12345;
  const std::string test_key = absl::StrCat("test_signed_val_", key_prefix());

  absl::SetFlag(&FLAGS_test_signed_val, test_signed_val);

  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Write(test_key, FLAGS_test_signed_val));
  ASSERT_TRUE(config_impl->Close());

  absl::SetFlag(&FLAGS_test_signed_val, 0);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_TRUE(config_impl->HasKey<int32_t>(test_key));
  EXPECT_TRUE(config_impl->HasKey<int64_t>(test_key));
  EXPECT_FALSE(config_impl->HasKey<std::string>(test_key));
  EXPECT_TRUE(config_impl->Read(test_key, &FLAGS_test_signed_val));
  ASSERT_TRUE(config_impl->Close());

  EXPECT_EQ(absl::GetFlag(FLAGS_test_signed_val), test_signed_val);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Delete(test_key));
  ASSERT_TRUE(config_impl->Close());

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_FALSE(config_impl->HasKey<int32_t>(test_key));
  EXPECT_FALSE(config_impl->HasKey<int64_t>(test_key));
  ASSERT_TRUE(config_impl->Close());
}

TEST_F(ConfigTest, RWUint32) {
  auto config_impl = config::ConfigImpl::Create();
  constexpr uint32_t test_unsigned_val = 12345U;
  const std::string test_key = absl::StrCat("test_unsigned_val_", key_prefix());

  absl::SetFlag(&FLAGS_test_unsigned_val, test_unsigned_val);

  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Write(test_key, FLAGS_test_unsigned_val));
  ASSERT_TRUE(config_impl->Close());

  absl::SetFlag(&FLAGS_test_unsigned_val, 0);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_TRUE(config_impl->HasKey<uint32_t>(test_key));
  EXPECT_TRUE(config_impl->HasKey<uint64_t>(test_key));
  EXPECT_FALSE(config_impl->HasKey<std::string>(test_key));
  EXPECT_TRUE(config_impl->Read(test_key, &FLAGS_test_unsigned_val));
  ASSERT_TRUE(config_impl->Close());

  EXPECT_EQ(absl::GetFlag(FLAGS_test_unsigned_val), test_unsigned_val);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Delete(test_key));
  ASSERT_TRUE(config_impl->Close());

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_FALSE(config_impl->HasKey<uint32_t>(test_key));
  EXPECT_FALSE(config_impl->HasKey<uint64_t>(test_key));
  ASSERT_TRUE(config_impl->Close());
}

TEST_F(ConfigTest, RWInt64) {
  auto config_impl = config::ConfigImpl::Create();
  constexpr int64_t test_signed_64val = -123456LL - INT32_MAX;
  const std::string test_key = absl::StrCat("test_signed_64val_", key_prefix());

  absl::SetFlag(&FLAGS_test_signed_64val, test_signed_64val);

  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Write(test_key, FLAGS_test_signed_64val));
  ASSERT_TRUE(config_impl->Close());

  absl::SetFlag(&FLAGS_test_signed_64val, 0);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_TRUE(config_impl->HasKey<int64_t>(test_key));
  EXPECT_FALSE(config_impl->HasKey<std::string>(test_key));
  EXPECT_TRUE(config_impl->Read(test_key, &FLAGS_test_signed_64val));
  ASSERT_TRUE(config_impl->Close());

  EXPECT_EQ(absl::GetFlag(FLAGS_test_signed_64val), test_signed_64val);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Delete(test_key));
  ASSERT_TRUE(config_impl->Close());

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_FALSE(config_impl->HasKey<int64_t>(test_key));
  ASSERT_TRUE(config_impl->Close());
}

TEST_F(ConfigTest, RWUint64) {
  auto config_impl = config::ConfigImpl::Create();
  constexpr uint64_t test_unsigned_64val = 123456ULL + UINT32_MAX;
  const std::string test_key = absl::StrCat("test_unsigned_64val_", key_prefix());

  absl::SetFlag(&FLAGS_test_unsigned_64val, test_unsigned_64val);

  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Write(test_key, FLAGS_test_unsigned_64val));
  ASSERT_TRUE(config_impl->Close());

  absl::SetFlag(&FLAGS_test_unsigned_64val, 0);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_TRUE(config_impl->HasKey<uint64_t>(test_key));
  EXPECT_FALSE(config_impl->HasKey<std::string>(test_key));
  EXPECT_TRUE(config_impl->Read(test_key, &FLAGS_test_unsigned_64val));
  ASSERT_TRUE(config_impl->Close());

  EXPECT_EQ(absl::GetFlag(FLAGS_test_unsigned_64val), test_unsigned_64val);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Delete(test_key));
  ASSERT_TRUE(config_impl->Close());

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_FALSE(config_impl->HasKey<uint64_t>(test_key));
  ASSERT_TRUE(config_impl->Close());
}

TEST_F(ConfigTest, RWString) {
  auto config_impl = config::ConfigImpl::Create();
  constexpr std::string_view test_string = "test-str";
  const std::string test_key = absl::StrCat("test_string_", key_prefix());

  absl::SetFlag(&FLAGS_test_string, test_string);

  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Write(test_key, FLAGS_test_string));
  ASSERT_TRUE(config_impl->Close());

  absl::SetFlag(&FLAGS_test_string, "");

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_TRUE(config_impl->HasKey<std::string>(test_key));
  EXPECT_FALSE(config_impl->HasKey<bool>(test_key));
  EXPECT_FALSE(config_impl->HasKey<uint32_t>(test_key));
  EXPECT_FALSE(config_impl->HasKey<uint64_t>(test_key));
  EXPECT_FALSE(config_impl->HasKey<int32_t>(test_key));
  EXPECT_FALSE(config_impl->HasKey<int64_t>(test_key));
  EXPECT_TRUE(config_impl->Read(test_key, &FLAGS_test_string));
  ASSERT_TRUE(config_impl->Close());

  EXPECT_EQ(absl::GetFlag(FLAGS_test_string), test_string);

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(true));
  EXPECT_TRUE(config_impl->Delete(test_key));
  ASSERT_TRUE(config_impl->Close());

  config_impl = config::ConfigImpl::Create();
  ASSERT_TRUE(config_impl->Open(false));
  EXPECT_FALSE(config_impl->HasKey<std::string>(test_key));
  ASSERT_TRUE(config_impl->Close());
}
