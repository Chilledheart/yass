// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG_CORE
#define H_CONFIG_CONFIG_CORE

#include <absl/flags/declare.h>
#include <absl/strings/string_view.h>
#include <base/compiler_specific.h>
#include <stdint.h>
#include <string>
#include <string_view>

#include "config/config_export.hpp"

ABSL_DECLARE_FLAG(std::string, server_host);
ABSL_DECLARE_FLAG(std::string, server_sni);
ABSL_DECLARE_FLAG(PortFlag, server_port);
ABSL_DECLARE_FLAG(std::string, username);
ABSL_DECLARE_FLAG(std::string, password);
ABSL_DECLARE_FLAG(CipherMethodFlag, method);
ABSL_DECLARE_FLAG(std::string, local_host);
ABSL_DECLARE_FLAG(PortFlag, local_port);

ABSL_DECLARE_FLAG(uint32_t, parallel_max);
ABSL_DECLARE_FLAG(RateFlag, limit_rate);  // bytes per second

bool AbslParseFlag(absl::string_view text, PortFlag* flag, std::string* err);

std::string AbslUnparseFlag(const PortFlag&);

bool AbslParseFlag(absl::string_view text, CipherMethodFlag* flag, std::string* err);

std::string AbslUnparseFlag(const CipherMethodFlag&);

bool AbslParseFlag(absl::string_view text, RateFlag* flag, std::string* err);

std::string AbslUnparseFlag(const RateFlag&);

#if BUILDFLAG(IS_MAC)
ABSL_DECLARE_FLAG(bool, ui_display_realtime_status);
#endif

#endif  // H_CONFIG_CONFIG_CORE
