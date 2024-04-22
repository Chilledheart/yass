// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "config/config.hpp"
#include "config/config_impl.hpp"

#include <absl/flags/internal/program_name.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <stdlib.h>
#include <iostream>
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "feature.h"
#include "version.h"

namespace config {

static void ParseConfigFileOption(int argc, const char** argv) {
  int pos = 1;
  while (pos < argc) {
    std::string arg = argv[pos];
    if (arg == "--ipv4") {
      absl::SetFlag(&FLAGS_ipv6_mode, false);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (arg == "--ipv6") {
      absl::SetFlag(&FLAGS_ipv6_mode, true);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (arg == "-k" || arg == "--k" || arg == "-insecure_mode" || arg == "--insecure_mode") {
      absl::SetFlag(&FLAGS_insecure_mode, true);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (arg == "-noinsecure_mode" || arg == "-insecure_mode=false" || arg == "--noinsecure_mode" ||
               arg == "--insecure_mode=false") {
      absl::SetFlag(&FLAGS_insecure_mode, false);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (pos + 1 < argc && (arg == "-c" || arg == "--configfile")) {
      /* deprecated */
      g_configfile = argv[pos + 1];
      argv[pos] = "";
      argv[pos + 1] = "";
      pos += 2;
      continue;
    } else if (pos + 1 < argc && (arg == "-K" || arg == "--config")) {
      g_configfile = argv[pos + 1];
      argv[pos] = "";
      argv[pos + 1] = "";
      pos += 2;
      continue;
    } else if (arg == "-version" || arg == "--version") {
      std::cout << absl::flags_internal::ShortProgramInvocationName() << " " << YASS_APP_TAG << std::endl;
      std::cout << "Last Change: " << YASS_APP_LAST_CHANGE << std::endl;
      std::cout << "Features: " << YASS_APP_FEATURES << std::endl;
#ifndef NDEBUG
      std::cout << "Debug build (NDEBUG not #defined)" << std::endl;
#endif
      argv[pos] = "";
      pos += 1;
      exit(0);
    }
    ++pos;
  }

  std::cerr << "Application starting: " << YASS_APP_TAG << std::endl;
  std::cerr << "Last Change: " << YASS_APP_LAST_CHANGE << std::endl;
  std::cerr << "Features: " << YASS_APP_FEATURES << std::endl;
#ifndef NDEBUG
  std::cerr << "Debug build (NDEBUG not #defined)\n" << std::endl;
#endif
}

void ReadConfigFileAndArguments(int argc, const char** argv) {
  ParseConfigFileOption(argc, argv);
  config::ReadConfig();
  if (argc) {
    absl::ParseCommandLine(argc, const_cast<char**>(argv));
  }

  // first line of logging
  LOG(WARNING) << "Application starting: " << YASS_APP_TAG << " type: " << ProgramTypeToStr(pType);
  LOG(WARNING) << "Last Change: " << YASS_APP_LAST_CHANGE;
  LOG(WARNING) << "Features: " << YASS_APP_FEATURES;
#ifndef NDEBUG
  LOG(WARNING) << "Debug build (NDEBUG not #defined)\n";
#endif
}

}  // namespace config
