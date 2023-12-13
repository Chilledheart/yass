// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef _WIN32

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>
#include <absl/flags/flag.h>
#include <build/build_config.h>

#include <gmock/gmock.h>

#include "test_util.hpp"
#include "core/process_utils.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"

#ifdef OS_ANDROID
ABSL_FLAG(bool, no_exec_proc_tests, true, "skip execute_process tests");
#else
ABSL_FLAG(bool, no_exec_proc_tests, false, "skip execute_process tests");
#endif

TEST(PROCESS_TEST, ExecuteProcessBasic) {
  if (absl::GetFlag(FLAGS_no_exec_proc_tests)) {
    GTEST_SKIP() << "skipped as required";
    return;
  }
  std::string main_exe;
  GetExecutablePath(&main_exe);
  std::vector<std::string> params = {main_exe.c_str(), "--version"};
  std::string output, error;
  int ret = ExecuteProcess(params, &output, &error);
  EXPECT_EQ(ret, 0);
  EXPECT_THAT(output, ::testing::StartsWith("yass_test\n"));
  EXPECT_EQ(error, "");
}

#endif // _WIN32
