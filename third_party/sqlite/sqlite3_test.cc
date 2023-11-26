// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>
#include <filesystem>

#include "core/process_utils.hpp"
#include "core/utils_fs.hpp"
#include "test_util.hpp"
#include <sqlite3.h>

using namespace yass;

enum SqliteStorageType {
  kMemory,
  kDisk,
};

static constexpr char kSqliteMainDatabaseName[] = "test-db";
// Magic path value telling sqlite3_open_v2() to open an in-memory database.
static constexpr char kSqliteOpenInMemoryPath[] = ":memory:";

static const char* SqliteStorageTypeToName(SqliteStorageType type) {
  switch (type) {
    case kMemory:
      return "Memory";
    case kDisk:
      return "Disk";
    default:
      return "(None)";
  }
}

static std::string SqliteStorageTypeToDbName(SqliteStorageType type) {
  switch (type) {
    case kMemory:
      return kSqliteOpenInMemoryPath;
    case kDisk:
      return std::string(kSqliteMainDatabaseName) + "-" + std::to_string(GetPID());
    default:
      return "(null)";
  }
}

static int SqliteOpenDB(SqliteStorageType type,
  const std::string& filename,  /* Database filename (UTF-8) */
  sqlite3 **ppDb,               /* OUT: SQLite db handle */
  int flags,                    /* Flags */
  const char *zVfs              /* Name of VFS module to use */
  ) {
  switch (type) {
    case kMemory:
      return sqlite3_open_v2(filename.c_str(), ppDb, flags | SQLITE_OPEN_MEMORY, zVfs);
    case kDisk:
      return sqlite3_open_v2(filename.c_str(), ppDb, flags & ~SQLITE_OPEN_MEMORY, zVfs);
    default:
      return SQLITE_ERROR;
  }
}

static void SqliteDestroyDB(SqliteStorageType type, const std::string& filename) {
  if (type == kMemory) {
    return;
  }
  ASSERT_TRUE(RemoveFile(filename)) << "Removing sqlite file failed";
}

class SqliteTest : public ::testing::TestWithParam<SqliteStorageType> {
 public:
  static void SetUpTestSuite() {
    ASSERT_EQ(SQLITE_OK, sqlite3_initialize());
  }

  static void TearDownTestSuite() {
    // nop
  }

  void SetUp() override {
    db = nullptr;
    // The flags are documented at https://www.sqlite.org/c3ref/open.html.
    //
    // Chrome uses SQLITE_OPEN_PRIVATECACHE because SQLite is used by many
    // disparate features with their own databases, and having separate page
    // caches makes it easier to reason about each feature's performance in
    // isolation.
    //
    // SQLITE_OPEN_EXRESCODE enables the full range of SQLite error codes. See
    // https://www.sqlite.org/rescode.html for details.
    ASSERT_EQ(SQLITE_OK, SqliteOpenDB(GetParam(),
                                      ::testing::TempDir() + SqliteStorageTypeToDbName(GetParam()), &db,
                                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                      SQLITE_OPEN_EXRESCODE | SQLITE_OPEN_PRIVATECACHE, nullptr));
  }

  void TearDown() override {
    // sqlite3_open_v2() will usually create a database connection handle, even
    // if an error occurs (see https://www.sqlite.org/c3ref/open.html).
    // Therefore, we'll clear `db_` immediately - particularly before triggering
    // an error callback which may check whether a database connection exists.
    if (db) {
      ASSERT_EQ(SQLITE_OK, sqlite3_close(db));
      db = nullptr;
    }
    ASSERT_NO_FATAL_FAILURE(SqliteDestroyDB(GetParam(), ::testing::TempDir() + SqliteStorageTypeToDbName(GetParam())));
  }

  sqlite3* db;
};

TEST_P(SqliteTest, OpenAndClose) {
  ASSERT_EQ(SQLITE_VERSION_NUMBER, sqlite3_libversion_number()) << sqlite3_libversion();
}

TEST_P(SqliteTest, InsertAndDelete) {
  ASSERT_EQ(SQLITE_OK, sqlite3_exec(db,
                                    "create table if not exists tbl5(name TEXT varchar(100));",
                                    nullptr, nullptr, nullptr)) << sqlite3_errmsg(db);

  constexpr char s[20] = "some string";

  sqlite3_stmt *stmt;
  const char *remaining;
  const char* sql = "INSERT INTO tbl5(name) VALUES (?);";
  ASSERT_EQ(SQLITE_OK, sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, &remaining)) << sqlite3_errmsg(db);
  ASSERT_TRUE(std::all_of(remaining, sql + strlen(sql), [](char c) { return std::isspace(c); }));
  ASSERT_EQ(SQLITE_OK, sqlite3_bind_text(stmt, 1, s, strlen(s), nullptr)) << sqlite3_errmsg(db);
  ASSERT_EQ(SQLITE_DONE, sqlite3_step(stmt)) << sqlite3_errmsg(db);
  ASSERT_EQ(SQLITE_OK, sqlite3_finalize(stmt)) << sqlite3_errmsg(db);

  sql = "SELECT name FROM tbl5 where name=?1;";
  ASSERT_EQ(SQLITE_OK, sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, &remaining)) << sqlite3_errmsg(db);
  ASSERT_TRUE(std::all_of(remaining, sql + strlen(sql), [](char c) { return std::isspace(c); }));
  ASSERT_EQ(SQLITE_OK, sqlite3_bind_text(stmt, 1, s, strlen(s), nullptr)) << sqlite3_errmsg(db);
  ASSERT_EQ(SQLITE_ROW, sqlite3_step(stmt)) << sqlite3_errmsg(db);
  ASSERT_EQ(SQLITE_TEXT, sqlite3_column_type(stmt, 0));
  ASSERT_STREQ(s, (const char*)sqlite3_column_text(stmt, 0));
  ASSERT_EQ(SQLITE_DONE, sqlite3_step(stmt)) << sqlite3_errmsg(db);
  ASSERT_EQ(SQLITE_OK, sqlite3_finalize(stmt)) << sqlite3_errmsg(db);

  sql = "DELETE FROM tbl5 where name=?1;";
  ASSERT_EQ(SQLITE_OK, sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, &remaining)) << sqlite3_errmsg(db);
  ASSERT_EQ(true, std::all_of(remaining, sql + strlen(sql), [](char c) { return std::isspace(c); }));
  ASSERT_EQ(SQLITE_OK, sqlite3_bind_text(stmt, 1, s, strlen(s), nullptr)) << sqlite3_errmsg(db);
  ASSERT_EQ(SQLITE_DONE, sqlite3_step(stmt)) << sqlite3_errmsg(db);
  ASSERT_EQ(SQLITE_OK, sqlite3_finalize(stmt)) << sqlite3_errmsg(db);

  sql = "SELECT name FROM tbl5 where name=?1;";
  ASSERT_EQ(SQLITE_OK, sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, &remaining)) << sqlite3_errmsg(db);
  ASSERT_TRUE(std::all_of(remaining, sql + strlen(sql), [](char c) { return std::isspace(c); }));
  ASSERT_EQ(SQLITE_OK, sqlite3_bind_text(stmt, 1, s, strlen(s), nullptr)) << sqlite3_errmsg(db);
  ASSERT_EQ(SQLITE_DONE, sqlite3_step(stmt)) << sqlite3_errmsg(db);
  ASSERT_EQ(SQLITE_OK, sqlite3_finalize(stmt)) << sqlite3_errmsg(db);
}

static constexpr SqliteStorageType kTestTypes[] = {
  kMemory,
  kDisk
};

INSTANTIATE_TEST_SUITE_P(ThirdParty, SqliteTest, ::testing::ValuesIn(kTestTypes),
  [](const ::testing::TestParamInfo<SqliteStorageType>& info) -> std::string {
    return SqliteStorageTypeToName(info.param);
  });
