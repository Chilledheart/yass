// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include "test_util.hpp"
#include <sqlite3.h>

class SqliteMemoryDB : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_EQ(SQLITE_OK, sqlite3_initialize());
    ASSERT_EQ(SQLITE_OK, sqlite3_open_v2("test-db", &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, nullptr));
    errmsg_ = nullptr;
  }

  void TearDown() override {
    if (errmsg_) {
      sqlite3_free(errmsg_);
      errmsg_ = nullptr;
    }
    ASSERT_EQ(SQLITE_OK, sqlite3_close(db_));
  }

  char** errmsg() {
    return &errmsg_;
  }

  sqlite3 *db() {
    return db_;
  }

 private:
  char* errmsg_ = nullptr;
  sqlite3* db_ = nullptr;
};

TEST_F(SqliteMemoryDB, OpenAndClose) {
  ASSERT_EQ(SQLITE_VERSION_NUMBER, sqlite3_libversion_number()) << sqlite3_libversion();
}

TEST_F(SqliteMemoryDB, InsertAndDelete) {
  ASSERT_EQ(SQLITE_OK, sqlite3_exec(db(),
                                    "create table if not exists tbl5(name TEXT varchar(100));",
                                    nullptr, nullptr, errmsg())) << *errmsg();
  ASSERT_EQ(*errmsg(), nullptr);

  char s[20] = "some string";
  char* query = sqlite3_mprintf("insert into tbl5 values ('%q');", s);
  ASSERT_EQ(SQLITE_OK, sqlite3_exec(db(), query,
                                    nullptr, nullptr, errmsg())) << *errmsg();
  sqlite3_free(query);
  ASSERT_EQ(*errmsg(), nullptr);

  query = sqlite3_mprintf("delete from tbl5 where name= ('%q');", s);
  ASSERT_EQ(SQLITE_OK, sqlite3_exec(db(), query,
                                    nullptr, nullptr, errmsg())) << *errmsg();
  sqlite3_free(query);
  ASSERT_EQ(*errmsg(), nullptr);
}
