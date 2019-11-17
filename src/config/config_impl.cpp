//
// config_impl.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
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
