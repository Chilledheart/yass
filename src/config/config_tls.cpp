// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "config/config.hpp"

std::string g_certificate_chain_content;
std::string g_private_key_content;

ABSL_FLAG(std::string, certificate_chain_file, "", "Certificate Chain File Path (Both of Server and Client)");
ABSL_FLAG(std::string, private_key_file, "", "Private Key File Path (Server Only)");
ABSL_FLAG(std::string, private_key_password, "", "Private Key Password (Server Only)");
ABSL_FLAG(bool, insecure_mode, false, "Or '-k', This option makes to skip the verification step and proceed without checking (Client Only)");
ABSL_FLAG(std::string, cacert, getenv("YASS_CA_BUNDLE") ? getenv("YASS_CA_BUNDLE") : "",
          "Tells where to use the specified certificate file to verify the peer. "
          "You can override it with YASS_CA_BUNDLE environment variable");
ABSL_FLAG(std::string, capath, "", "Tells where to use the specified certificate directory to verify the peer.");
