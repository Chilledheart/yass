// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>
#include <filesystem>

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

static const char* SqliteStorageTypeToDbName(SqliteStorageType type) {
  switch (type) {
    case kMemory:
      return kSqliteOpenInMemoryPath;
    case kDisk:
      return kSqliteMainDatabaseName;
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
  std::error_code ec;
  std::filesystem::path p(filename);
  std::filesystem::remove(p, ec);
  ASSERT_FALSE(ec) << "Removing sqlite file failed: " << ec;
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
    errmsg = nullptr;
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
    if (errmsg) {
      sqlite3_free(errmsg);
      errmsg = nullptr;
    }
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

  char* errmsg;
  sqlite3* db;
};

TEST_P(SqliteTest, OpenAndClose) {
  ASSERT_EQ(SQLITE_VERSION_NUMBER, sqlite3_libversion_number()) << sqlite3_libversion();
}

TEST_P(SqliteTest, InsertAndDelete) {
  ASSERT_EQ(SQLITE_OK, sqlite3_exec(db,
                                    "create table if not exists tbl5(name TEXT varchar(100));",
                                    nullptr, nullptr, &errmsg)) << errmsg;
  ASSERT_EQ(errmsg, nullptr);

  char s[20] = "some string";
  char* query = sqlite3_mprintf("insert into tbl5 values ('%q');", s);
  ASSERT_EQ(SQLITE_OK, sqlite3_exec(db, query,
                                    nullptr, nullptr, &errmsg)) << errmsg;
  sqlite3_free(query);
  ASSERT_EQ(errmsg, nullptr);

  query = sqlite3_mprintf("delete from tbl5 where name= ('%q');", s);
  ASSERT_EQ(SQLITE_OK, sqlite3_exec(db, query,
                                    nullptr, nullptr, &errmsg)) << errmsg;
  sqlite3_free(query);
  ASSERT_EQ(errmsg, nullptr);
}

static constexpr SqliteStorageType kTestTypes[] = {
  kMemory,
  kDisk
};

INSTANTIATE_TEST_SUITE_P(ThirdParty, SqliteTest, ::testing::ValuesIn(kTestTypes),
  [](const ::testing::TestParamInfo<SqliteStorageType>& info) -> std::string {
    return SqliteStorageTypeToName(info.param);
  });
