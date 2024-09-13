// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#include "config/config_ptype.hpp"

#include "gui_variant.h"

namespace config {

const char* ProgramTypeToStr(ProgramType type) {
  switch (type) {
    case YASS_SERVER_DEFAULT:
      return "server";
    case YASS_UNITTEST_DEFAULT:
      return "unittest";
    case YASS_BENCHMARK_DEFAULT:
      return "benchmark";
    case YASS_CLIENT_DEFAULT:
      return "client";
    case YASS_CLIENT_GUI:
      return "gui client (" YASS_GUI_FLAVOUR ")";
    case YASS_UNSPEC:
    default:
      return "unspec";
  }
}

}  // namespace config
