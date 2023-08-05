// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef _WIN32

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include <gmock/gmock.h>

#include "test_util.hpp"
#include "core/process_utils.hpp"
#include "core/logging.hpp"

TEST(PROCESS_TEST, ExecuteProcessBasic) {
  std::vector<std::string> params = {"/bin/echo", "-n", "cAsHcOw"};
  std::string output, error;
  int ret = ExecuteProcess(params, &output, &error);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(output, "cAsHcOw");
  EXPECT_EQ(error, "");
}

#endif // _WIN32
