// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include "test_util.hpp"
#include <sqlite3.h>

TEST(SqliteMemoryDB, OpenAndClose) {
  sqlite3* db;
  ASSERT_EQ(SQLITE_OK, sqlite3_initialize());
  ASSERT_EQ(SQLITE_OK, sqlite3_open_v2("test-db", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, nullptr));
  ASSERT_EQ(SQLITE_OK, sqlite3_close(db));
}

TEST(SqliteMemoryDB, InsertAndDelete) {
  sqlite3* db;
  char *errmsg;
  ASSERT_EQ(SQLITE_OK, sqlite3_initialize());
  ASSERT_EQ(SQLITE_OK, sqlite3_open_v2("test-db", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, nullptr));

  ASSERT_EQ(SQLITE_OK, sqlite3_exec(db, "create table if not exists tbl5(name TEXT varchar(100));", nullptr, nullptr, &errmsg));
  if (errmsg) {
    sqlite3_free(errmsg);
  }

  char s[20] = "some string";
  char* query = sqlite3_mprintf("insert into tbl5 values ('%q');", s);
  ASSERT_EQ(SQLITE_OK, sqlite3_exec(db, query, nullptr, nullptr, &errmsg));
  sqlite3_free(query);

  query = sqlite3_mprintf("delete from tbl5 where name= ('%q');", s);
  ASSERT_EQ(SQLITE_OK, sqlite3_exec(db, query, nullptr, nullptr, &errmsg));
  sqlite3_free(query);

  ASSERT_EQ(SQLITE_OK, sqlite3_close(db));
}
