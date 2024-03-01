// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <gtest/gtest.h>

#include "core/logging.hpp"

#ifdef __clang__
TEST(CompilerRegression, ClangLibCppBoolalpha) {
  for (int i = 0; i < 100; ++i) {
    LOG(WARNING) << "yes";
    LOG(WARNING) << 1;
    LOG(WARNING) << true;
    LOG(WARNING) << std::boolalpha;
    LOG(WARNING) << std::noboolalpha;
    LOG(WARNING) << std::boolalpha << true;
    LOG(WARNING) << std::noboolalpha << true;
    LOG(WARNING) << std::boolalpha << true << std::noboolalpha;
  }
}
#endif
