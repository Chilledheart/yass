// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>

#include "core/utils.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

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
