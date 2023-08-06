// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef _WIN32

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include <gmock/gmock.h>

#include "test_util.hpp"
#include "core/process_utils.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"

TEST(PROCESS_TEST, ExecuteProcessBasic) {
  std::string main_exe;
  GetExecutablePath(&main_exe);
  std::vector<std::string> params = {main_exe.c_str(), "--version"};
  std::string output, error;
  int ret = ExecuteProcess(params, &output, &error);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(output, "yass_test\n");
  EXPECT_EQ(error, "");
}

#endif // _WIN32
