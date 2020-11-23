// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "config/config_impl.hpp"
#include <gflags/gflags.h>
#include "config/config_impl_posix.hpp"
#include "config/config_impl_windows.hpp"

namespace config {

ConfigImpl::~ConfigImpl() {}

std::unique_ptr<ConfigImpl> ConfigImpl::Create() {
#ifdef _WIN32
  return std::make_unique<ConfigImplWindows>();
#else
  return std::make_unique<ConfigImplPosix>();
#endif
}

} // namespace config
