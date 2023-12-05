// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>

#include "core/logging.hpp"
#include "core/process_utils.hpp"
#include "core/rand_util.hpp"
#include "core/utils.hpp"
#include "core/utils_fs.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <absl/strings/str_format.h>
#include <filesystem>

using namespace yass;

TEST(UtilsTest, Dirname) {
  ASSERT_EQ(Dirname("a/b/prog/file.cc"), "a/b/prog");
  ASSERT_EQ(Dirname("a/b/prog//"), "a/b");
  ASSERT_EQ(Dirname("file.cc"), ".");
  ASSERT_EQ(Dirname("/file.cc"), "/");
  ASSERT_EQ(Dirname("//file.cc"), "/");
  ASSERT_EQ(Dirname("/dir//file.cc"), "/dir");
}

TEST(UtilsTest, Basename) {
  ASSERT_EQ(Basename("a/b/prog/file.cc"), "file.cc");
  ASSERT_EQ(Basename("a/b/prog//"), "prog");
  ASSERT_EQ(Basename("file.cc"), "file.cc");
  ASSERT_EQ(Basename("/file.cc"), "file.cc");
  ASSERT_EQ(Basename("//file.cc"), "file.cc");
  ASSERT_EQ(Basename("/dir//file.cc"), "file.cc");
  ASSERT_EQ(Basename("////"), "/");
  ASSERT_EQ(Basename("c/"), "c");
  ASSERT_EQ(Basename("/a/b/c"), "c");
}

#ifdef _WIN32
TEST(UtilsTest, ExpandUserFromStringImpl) {
  wchar_t path[] = L"C:/path/to/directory";
  size_t path_len = sizeof(path) / sizeof(path[0]);
  // the return value is the REQUIRED number of TCHARs,
  // including the terminating NULL character.
  DWORD required_size = ::ExpandEnvironmentStringsW(path, nullptr, 0);

  /* if failure or too many bytes required, documented in
   * ExpandEnvironmentStringsW */
  ASSERT_NE(0u, required_size);

  std::wstring expanded_path;
  expanded_path.resize(required_size-1);
  ASSERT_EQ(path_len, required_size);
  /* the buffer size should be the string length plus the terminating null character */
  ::ExpandEnvironmentStringsW(path, &expanded_path[0], required_size);

  ASSERT_STREQ(path, expanded_path.c_str());
  ASSERT_EQ(std::wstring(path, path_len-1), expanded_path);
}

TEST(UtilsTest, ExpandUserFromString) {
  wchar_t path[] = L"%TEMP%/path/to/directory";
  size_t path_len = sizeof(path) / sizeof(path[0]);

  wchar_t temp[32767];
  DWORD temp_len = GetEnvironmentVariableW(L"TEMP", temp, sizeof(temp) / sizeof(temp[0]));
  // GetEnvironmentVariableW: the return value is the number of characters
  // stored in the buffer pointed to by lpBuffer, not including the terminating null character.
  ASSERT_NE(0u, temp_len);
  std::wstring expected_expanded_path = std::wstring(temp, temp_len) + L"/path/to/directory";

  ASSERT_EQ(expected_expanded_path, ExpandUserFromString(path, path_len));
}
#endif

TEST(UtilsTest, StringToInteger) {
  auto i = StringToInteger("123");
  ASSERT_TRUE(i.ok()) << i.status();
  ASSERT_EQ(i.value(), 123);

  const char s3[] = "123";
  i = StringToInteger(std::string(s3, 3));
  ASSERT_TRUE(i.ok()) << i.status();
  ASSERT_EQ(i.value(), 123);

  i = StringToInteger(std::string(s3, 4));
  ASSERT_FALSE(i.ok()) << i.status();

  const char s4[] = "123a";

  i = StringToInteger(std::string(s4, 4));
  ASSERT_FALSE(i.ok()) << i.status();
}

TEST(UtilsTest, GetTempDir) {
  std::string tmp_dir;
  ASSERT_TRUE(GetTempDir(&tmp_dir));
  ASSERT_FALSE(tmp_dir.empty());
  LOG(ERROR) << "tmp_dir: " << tmp_dir;
}

TEST(UtilsTest, ReadFileAndWrite4K) {
  std::string buf, buf2;
  buf.resize(4096);
  buf2.resize(4096);
  RandBytes(buf.data(), buf.size());
  int tmp_suffix;
  RandBytes(&tmp_suffix, sizeof(tmp_suffix));
  auto tmp_name = absl::StrFormat("read_write_file-%u-%d", GetPID(), tmp_suffix);
  auto tmp_dir = std::filesystem::path(::testing::TempDir());
#ifdef _WIN32
  std::string tmp = SysWideToUTF8(tmp_dir / tmp_name);
#else
  std::string tmp = tmp_dir / tmp_name;
#endif

  ASSERT_TRUE(WriteFileWithBuffer(tmp, buf.c_str(), buf.size()));
  ASSERT_TRUE(ReadFileToBuffer(tmp, buf2.data(), buf2.size()+1));
  ASSERT_EQ(buf, buf2);

  ASSERT_TRUE(RemoveFile(tmp));
}
