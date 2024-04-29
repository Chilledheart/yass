// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#include "config/config.hpp"

#include <absl/flags/flag.h>
#include <iostream>
#include "core/utils.hpp"

std::string g_certificate_chain_content;
std::string g_private_key_content;

ABSL_FLAG(std::string, certificate_chain_file, "", "Certificate Chain File Path (Both of Server and Client)");
ABSL_FLAG(std::string, private_key_file, "", "Private Key File Path (Server Only)");
ABSL_FLAG(std::string, private_key_password, "", "Private Key Password (Server Only)");
ABSL_FLAG(bool,
          insecure_mode,
          false,
          "Or '-k', This option makes to skip the verification step and proceed without checking (Client Only)");
ABSL_FLAG(std::string,
          cacert,
          getenv("YASS_CA_BUNDLE") ? getenv("YASS_CA_BUNDLE") : "",
          "Tells where to use the specified certificate file to verify the peer. "
          "You can override it with YASS_CA_BUNDLE environment variable");
ABSL_FLAG(std::string, capath, "", "Tells where to use the specified certificate directory to verify the peer.");

namespace config {
bool ReadTLSConfigFile() {
  do {
    static constexpr const size_t kBufferSize = 256 * 1024;
    const bool is_server = pType == YASS_SERVER;

    ssize_t ret;
    if (is_server) {
      std::string private_key, private_key_path = absl::GetFlag(FLAGS_private_key_file);
      if (private_key_path.empty()) {
        std::cerr << "No private key file for certificate provided" << std::endl;
        return false;
      }
      private_key.resize(kBufferSize);
      ret = ReadFileToBuffer(private_key_path, absl::MakeSpan(private_key));
      if (ret <= 0) {
        std::cerr << "private key " << private_key_path << " failed to read" << std::endl;
        return -1;
      }
      private_key.resize(ret);
      g_private_key_content = private_key;
      std::cerr << "Using private key file: " << private_key_path << std::endl;
    }
    std::string certificate_chain, certificate_chain_path = absl::GetFlag(FLAGS_certificate_chain_file);
    if (is_server && certificate_chain_path.empty()) {
      std::cerr << "No certificate file provided" << std::endl;
      return false;
    }
    if (!certificate_chain_path.empty()) {
      certificate_chain.resize(kBufferSize);
      ret = ReadFileToBuffer(certificate_chain_path, absl::MakeSpan(certificate_chain));
      if (ret <= 0) {
        std::cerr << "certificate file " << certificate_chain_path << " failed to read" << std::endl;
        return false;
      }
      certificate_chain.resize(ret);
      g_certificate_chain_content = certificate_chain;
      std::cerr << "Using certificate chain file: " << certificate_chain_path << std::endl;
    }
  } while (false);
  return true;
}
}  // namespace config
