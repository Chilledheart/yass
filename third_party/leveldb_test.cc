// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

#include "core/logging.hpp"
#include "core/process_utils.hpp"
#include "core/rand_util.hpp"

static std::string RandString(size_t length) {
  std::string ret;
  ret.resize(length, 0);
  RandBytes(ret.data(), length);
  return ret;
}

static const char* LevelDBCompressionTypeToName(leveldb::CompressionType type) {
  switch(type) {
    case leveldb::kNoCompression:
      return "NoCompression";
    case leveldb::kSnappyCompression:
      return "SnappyCompression";
    case leveldb::kZstdCompression:
      return "ZstdCompression";
    default:
      return "InvalidCompression";
  }
}

static std::string LevelDBCompressionTypeToDBName(leveldb::CompressionType type) {
  switch(type) {
    case leveldb::kNoCompression:
      return std::string("test-ldb-no") + "-" + std::to_string(GetPID());
    case leveldb::kSnappyCompression:
      return std::string("test-ldb-snappy") + "-" + std::to_string(GetPID());
    case leveldb::kZstdCompression:
      return std::string("test-ldb-zstd") + "-" + std::to_string(GetPID());
    default:
      return std::string("test-ldb-invalid") + "-" + std::to_string(GetPID());
  }
}


class LevelDBTest : public ::testing::TestWithParam<leveldb::CompressionType> {
 public:
  void SetUp() override {
    db = nullptr;
    leveldb::Options options;
    options.compression = GetParam();
    options.create_if_missing = true;
    auto status = leveldb::DB::Open(options, ::testing::TempDir() + LevelDBCompressionTypeToDBName(GetParam()), &db);
    ASSERT_TRUE(status.ok()) << status.ToString();
  }

  void TearDown() override {
    delete db;

    leveldb::Options options;
    options.compression = GetParam();
    options.create_if_missing = true;
    auto status = leveldb::DestroyDB(::testing::TempDir() + LevelDBCompressionTypeToDBName(GetParam()), options);
    ASSERT_TRUE(status.ok()) << status.ToString();
  }

  leveldb::DB *db;
};

TEST_P(LevelDBTest, GetStats) {
  std::string stats;
  ASSERT_TRUE(db->GetProperty("leveldb.stats", &stats));
  LOG(ERROR) << stats;
}

TEST_P(LevelDBTest, GetNotFound) {
  leveldb::ReadOptions read_options;
  std::string output;
  auto status = db->Get(read_options, "test-key", &output);
  ASSERT_TRUE(status.IsNotFound()) << status.ToString();
}

TEST_P(LevelDBTest, DeleteNotFound) {
  leveldb::WriteOptions write_options;
  auto status = db->Delete(write_options, "test-key");
  ASSERT_TRUE(status.ok()) << status.ToString();
}

TEST_P(LevelDBTest, PutAndGet4096Byte) {
  std::string test_value = RandString(4096);
  leveldb::WriteOptions write_options;
  auto status = db->Put(write_options, "test-key", test_value);
  ASSERT_TRUE(status.ok()) << status.ToString();

  leveldb::ReadOptions read_options;
  std::string output;
  status = db->Get(read_options, "test-key", &output);
  ASSERT_TRUE(status.ok()) << status.ToString();
  ASSERT_STREQ(test_value.c_str(), output.c_str());
}

TEST_P(LevelDBTest, PutAndDelete4096Byte) {
  std::string test_value = RandString(4096);
  leveldb::WriteOptions write_options;
  auto status = db->Put(write_options, "test-key", test_value);
  ASSERT_TRUE(status.ok()) << status.ToString();

  status = db->Delete(write_options, "test-key");
  ASSERT_TRUE(status.ok()) << status.ToString();

  leveldb::ReadOptions read_options;
  std::string output;
  status = db->Get(read_options, "test-key", &output);
  ASSERT_TRUE(status.IsNotFound()) << status.ToString();
}

static constexpr leveldb::CompressionType kCompressions[] = {
  leveldb::kNoCompression,
  leveldb::kSnappyCompression,
  leveldb::kZstdCompression,
};

INSTANTIATE_TEST_SUITE_P(ThirdParty, LevelDBTest, ::testing::ValuesIn(kCompressions),
  [](const ::testing::TestParamInfo<leveldb::CompressionType>& info) -> std::string {
     return LevelDBCompressionTypeToName(info.param);
  });
