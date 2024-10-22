// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#include <gtest/gtest.h>

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "core/utils_fs.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <absl/strings/str_format.h>
#include <base/process/process_handle.h>
#include <base/rand_util.h>
#include <filesystem>
#include <string_view>

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
  std::wstring path = L"C:/path/to/directory";
  const size_t path_len = path.size();
  // the return value is the REQUIRED number of TCHARs,
  // including the terminating NULL character.
  DWORD required_size = ::ExpandEnvironmentStringsW(path.c_str(), nullptr, 0);

  /* if failure or too many bytes required, documented in
   * ExpandEnvironmentStringsW */
  ASSERT_NE(0u, required_size);

  std::wstring expanded_path;
  expanded_path.resize(required_size - 1);
  ASSERT_EQ(path_len, required_size - 1);
  /* the buffer size should be the string length plus the terminating null character */
  ::ExpandEnvironmentStringsW(path.c_str(), &expanded_path[0], required_size);

  ASSERT_EQ(path, expanded_path);
}

TEST(UtilsTest, ExpandUserFromString) {
  std::wstring path = L"%TEMP%/path/to/directory";

  wchar_t temp[32767];
  DWORD temp_len = GetEnvironmentVariableW(L"TEMP", temp, std::size(temp));
  // GetEnvironmentVariableW: the return value is the number of characters
  // stored in the buffer pointed to by lpBuffer, not including the terminating null character.
  ASSERT_NE(0u, temp_len);
  std::wstring expected_expanded_path = std::wstring(temp, temp_len) + L"/path/to/directory";

  ASSERT_EQ(expected_expanded_path, ExpandUserFromString(path));
}
#endif

TEST(UtilsTest, GetTempDir) {
  std::string tmp_dir;
  ASSERT_TRUE(GetTempDir(&tmp_dir));
  ASSERT_FALSE(tmp_dir.empty());
  LOG(WARNING) << "tmp_dir: " << tmp_dir;
}

TEST(UtilsTest, GetHomeDir) {
  std::string home_dir = GetHomeDir();
  ASSERT_FALSE(home_dir.empty());
  LOG(WARNING) << "home_dir: " << home_dir;
}

TEST(UtilsTest, ReadFileAndWrite4K) {
  std::string buf, buf2;
  buf.resize(4096);
  gurl_base::RandBytes(buf.data(), buf.size());
  int tmp_suffix;
  gurl_base::RandBytes(&tmp_suffix, sizeof(tmp_suffix));
  auto tmp_name = absl::StrFormat("read_write_file-%u-%d", gurl_base::GetCurrentProcId(), tmp_suffix);
  auto tmp_dir = std::filesystem::path(::testing::TempDir());
#ifdef _WIN32
  std::string tmp = SysWideToUTF8(tmp_dir / tmp_name);
#else
  std::string tmp = tmp_dir / tmp_name;
#endif

  ASSERT_EQ(WriteFileWithBuffer(tmp, buf), (ssize_t)buf.size());

  // undersize
  buf2.resize(buf.size() / 2);
  ASSERT_EQ(ReadFileToBuffer(tmp, as_writable_bytes(make_span(buf2))), (ssize_t)buf.size() / 2);
  // don't need to resize buf2
  ASSERT_EQ(std::string_view(buf).substr(0, buf.size() / 2), std::string_view(buf2).substr(0, buf.size() / 2));

  // equalsize
  buf2.resize(buf.size());
  ASSERT_EQ(ReadFileToBuffer(tmp, as_writable_bytes(make_span(buf2))), (ssize_t)buf.size());
  // don't need to resize buf2
  ASSERT_EQ(buf, buf2);

  // oversize
  buf2.resize(buf.size() * 2);
  ASSERT_EQ(ReadFileToBuffer(tmp, as_writable_bytes(make_span(buf2))), (ssize_t)buf.size());
  buf2.resize(buf.size());
  ASSERT_EQ(buf, buf2);

  ASSERT_TRUE(RemoveFile(tmp));
}

TEST(UtilsTest, HumanReadableByteCountBin) {
  std::ostringstream ss;

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, 1);
  EXPECT_EQ(ss.str(), "1 B");

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, 1 << 10);
  EXPECT_EQ(ss.str(), " 1.00 K");

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, 1 << 20);
  EXPECT_EQ(ss.str(), " 1.00 M");

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, 1 << 30);
  EXPECT_EQ(ss.str(), " 1.00 G");

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, 1ULL << 40);
  EXPECT_EQ(ss.str(), " 1.00 T");

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, 1ULL << 50);
  EXPECT_EQ(ss.str(), " 1.00 P");

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, 1ULL << 60);
  EXPECT_EQ(ss.str(), " 1.00 E");

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, 8ULL << 60);
  EXPECT_EQ(ss.str(), " 8.00 E");

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, 15 * (1ULL << 60));
  EXPECT_EQ(ss.str(), "15.00 E");

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, 15.99 * (1ULL << 60));
  EXPECT_EQ(ss.str(), "15.99 E");

  ss.str("");
  ss.clear();
  HumanReadableByteCountBin(&ss, ~0ULL);
  EXPECT_EQ(ss.str(), "16.00 E");
}

TEST(UtilsTest, SplitHostPort) {
  std::string host;
  uint16_t portnum;

  // test with no port
  EXPECT_TRUE(SplitHostPortWithDefaultPort<80>(&host, &portnum, "localhost"));
  EXPECT_EQ(host, "localhost");
  EXPECT_EQ(portnum, 80u);

  // test with explicit port
  EXPECT_TRUE(SplitHostPortWithDefaultPort<80>(&host, &portnum, "localhost:12345"));
  EXPECT_EQ(host, "localhost");
  EXPECT_EQ(portnum, 12345u);

  // test with another explicit portnum
  EXPECT_TRUE(SplitHostPortWithDefaultPort<80>(&host, &portnum, "localhost:443"));
  EXPECT_EQ(host, "localhost");
  EXPECT_EQ(portnum, 443u);

  // test with username and password
  EXPECT_FALSE(SplitHostPortWithDefaultPort<80>(&host, &portnum, "username@localhost:443"));
  EXPECT_FALSE(SplitHostPortWithDefaultPort<80>(&host, &portnum, "username:password@localhost:443"));

  // test with invalid host
  EXPECT_FALSE(SplitHostPortWithDefaultPort<80>(&host, &portnum, ":443"));

  // test with invalid portnums
  EXPECT_FALSE(SplitHostPortWithDefaultPort<80>(&host, &portnum, "localhost:portnum"));
  EXPECT_FALSE(SplitHostPortWithDefaultPort<80>(&host, &portnum, "localhost:222222"));
  EXPECT_FALSE(SplitHostPortWithDefaultPort<80>(&host, &portnum, "localhost:-1"));

  // test with ipv4 address
  EXPECT_TRUE(SplitHostPortWithDefaultPort<80>(&host, &portnum, "127.0.0.1:443"));
  EXPECT_EQ(host, "127.0.0.1");
  EXPECT_EQ(portnum, 443u);

  // test with ipv6 address
  EXPECT_TRUE(SplitHostPortWithDefaultPort<80>(&host, &portnum, "[::1]:443"));
  EXPECT_EQ(host, "[::1]");
  EXPECT_EQ(portnum, 443u);
}
