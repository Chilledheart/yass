// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#include "config/config.hpp"

#include <absl/flags/flag.h>
#include <absl/flags/usage.h>
#include <absl/strings/str_cat.h>
#include <sstream>

#include "config/config_impl.hpp"
#include "core/utils.hpp"
#include "url/gurl.h"

#ifndef TLSEXT_MAXLEN_host_name
#define TLSEXT_MAXLEN_host_name 255
#endif

namespace config {

bool ReadConfig() {
  auto config_impl = config::ConfigImpl::Create();
  bool required_fields_loaded = true;
  bool client_required_fields_loaded = true;

  if (!config_impl->Open(false)) {
    if (config_impl->GetEnforceRead()) {
      exit(-1);
    }
    return false;
  }

  /* load required fields */
  required_fields_loaded &= config_impl->Read("server", &FLAGS_server_host);
  required_fields_loaded &= config_impl->Read("server_port", &FLAGS_server_port);
  required_fields_loaded &= config_impl->Read("method", &FLAGS_method);
  required_fields_loaded &= config_impl->Read("username", &FLAGS_username);
  required_fields_loaded &= config_impl->Read("password", &FLAGS_password, true);
  client_required_fields_loaded &= config_impl->Read("local", &FLAGS_local_host);
  client_required_fields_loaded &= config_impl->Read("local_port", &FLAGS_local_port);

  if (pType == YASS_CLIENT || pType == YASS_CLIENT_SLAVE) {
    required_fields_loaded &= client_required_fields_loaded;
  }

  /* optional fields */
  config_impl->Read("server_sni", &FLAGS_server_sni);

  config_impl->Read("fast_open", &FLAGS_tcp_fastopen);
  config_impl->Read("fast_open_connect", &FLAGS_tcp_fastopen_connect);

  config_impl->Read("congestion_algorithm", &FLAGS_congestion_algorithm);
  config_impl->Read("doh_url", &FLAGS_doh_url);
  config_impl->Read("dot_host", &FLAGS_dot_host);
  config_impl->Read("connect_timeout", &FLAGS_connect_timeout);
  config_impl->Read("tcp_nodelay", &FLAGS_tcp_nodelay);

  config_impl->Read("tcp_keep_alive", &FLAGS_tcp_keep_alive);
  config_impl->Read("tcp_keep_alive_cnt", &FLAGS_tcp_keep_alive_cnt);
  config_impl->Read("tcp_keep_alive_idle_timeout", &FLAGS_tcp_keep_alive_idle_timeout);
  config_impl->Read("tcp_keep_alive_interval", &FLAGS_tcp_keep_alive_interval);

  /* optional tls fields */
  config_impl->Read("certificate_chain_file", &FLAGS_certificate_chain_file);
  config_impl->Read("private_key_file", &FLAGS_private_key_file);
  config_impl->Read("private_key_password", &FLAGS_private_key_password, true);
  config_impl->Read("insecure_mode", &FLAGS_insecure_mode);
  config_impl->Read("cacert", &FLAGS_cacert);
  config_impl->Read("capath", &FLAGS_capath);
  config_impl->Read("tls13_early_data", &FLAGS_tls13_early_data);
  config_impl->Read("enable_post_quantum_kyber", &FLAGS_enable_post_quantum_kyber);

  /* close fields */
  config_impl->Close();

  /* check enforced options */
  if (!required_fields_loaded) {
    if (config_impl->GetEnforceRead()) {
      exit(-1);
    }
  }

  /* correct options */
  absl::SetFlag(&FLAGS_connect_timeout, std::max(0, absl::GetFlag(FLAGS_connect_timeout)));

  absl::SetFlag(&FLAGS_tcp_keep_alive_cnt, std::max(0, absl::GetFlag(FLAGS_tcp_keep_alive_cnt)));
  absl::SetFlag(&FLAGS_tcp_keep_alive_idle_timeout, std::max(0, absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout)));
  absl::SetFlag(&FLAGS_tcp_keep_alive_interval, std::max(0, absl::GetFlag(FLAGS_tcp_keep_alive_interval)));

  return required_fields_loaded;
}

bool SaveConfig() {
  auto config_impl = config::ConfigImpl::Create();
  bool all_fields_written = false;

  if (!config_impl->Open(true)) {
    return false;
  }

  all_fields_written &= config_impl->Write("server", FLAGS_server_host);
  all_fields_written &= config_impl->Write("server_sni", FLAGS_server_sni);
  all_fields_written &= config_impl->Write("server_port", FLAGS_server_port);
  all_fields_written &= config_impl->Write("method", FLAGS_method);
  all_fields_written &= config_impl->Write("username", FLAGS_username);
  all_fields_written &= config_impl->Write("password", FLAGS_password, true);
  all_fields_written &= config_impl->Write("local", FLAGS_local_host);
  all_fields_written &= config_impl->Write("local_port", FLAGS_local_port);

  all_fields_written &= config_impl->Write("fast_open", FLAGS_tcp_fastopen);
  all_fields_written &= config_impl->Write("fast_open_connect", FLAGS_tcp_fastopen_connect);
  static_cast<void>(config_impl->Delete("threads"));
  all_fields_written &= config_impl->Write("congestion_algorithm", FLAGS_congestion_algorithm);
  all_fields_written &= config_impl->Write("doh_url", FLAGS_doh_url);
  all_fields_written &= config_impl->Write("dot_host", FLAGS_dot_host);
  all_fields_written &= config_impl->Write("timeout", FLAGS_connect_timeout);
  all_fields_written &= config_impl->Write("connect_timeout", FLAGS_connect_timeout);
  all_fields_written &= config_impl->Write("tcp_nodelay", FLAGS_tcp_nodelay);

  all_fields_written &= config_impl->Write("tcp_keep_alive", FLAGS_tcp_keep_alive);
  all_fields_written &= config_impl->Write("tcp_keep_alive_cnt", FLAGS_tcp_keep_alive_cnt);
  all_fields_written &= config_impl->Write("tcp_keep_alive_idle_timeout", FLAGS_tcp_keep_alive_idle_timeout);
  all_fields_written &= config_impl->Write("tcp_keep_alive_interval", FLAGS_tcp_keep_alive_interval);

  all_fields_written &= config_impl->Write("certificate_chain_file", FLAGS_certificate_chain_file);
  all_fields_written &= config_impl->Write("private_key_file", FLAGS_private_key_file);
  all_fields_written &= config_impl->Write("private_key_password", FLAGS_private_key_password);
  all_fields_written &= config_impl->Write("insecure_mode", FLAGS_insecure_mode);
  all_fields_written &= config_impl->Write("cacert", FLAGS_cacert);
  all_fields_written &= config_impl->Write("capath", FLAGS_capath);
  all_fields_written &= config_impl->Write("tls13_early_data", FLAGS_tls13_early_data);
  all_fields_written &= config_impl->Write("enable_post_quantum_kyber", FLAGS_enable_post_quantum_kyber);

  all_fields_written &= config_impl->Close();

  return all_fields_written;
}

std::string ReadConfigFromArgument(std::string_view server_host,
                                   std::string_view server_sni,
                                   std::string_view _server_port,
                                   std::string_view username,
                                   std::string_view password,
                                   cipher_method method,
                                   std::string_view local_host,
                                   std::string_view _local_port,
                                   std::string_view doh_url,
                                   std::string_view dot_host,
                                   std::string_view _timeout) {
  std::ostringstream err_msg;

  if (server_host.empty() || server_host.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Server Host: " << server_host;
  }

  if (server_sni.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Server Host: " << server_sni;
  }

  auto server_port = StringToIntegerU(_server_port);
  if (!server_port.has_value() || server_port.value() == 0u || server_port.value() > 65535u) {
    err_msg << ",Invalid Server Port: " << _server_port;
  }

  if (method == CRYPTO_INVALID) {
    err_msg << ",Invalid Cipher: " << to_cipher_method_str(method);
  }

  if (local_host.empty() || local_host.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Local Host: " << local_host;
  }

  auto local_port = StringToIntegerU(_local_port);
  if (!local_port.has_value() || local_port.value() > 65535u) {
    err_msg << ",Invalid Local Port: " << _local_port;
  }

  if (!doh_url.empty()) {
    GURL url(doh_url);
    if (!url.is_valid() || !url.has_host() || !url.has_scheme() || url.scheme() != "https") {
      err_msg << ",Invalid DoH URL: " << doh_url;
    }
    if (!dot_host.empty()) {
      err_msg << ",Conflicting DoT Host: " << dot_host;
    }
  }

  if (!dot_host.empty()) {
    if (dot_host.size() >= TLSEXT_MAXLEN_host_name) {
      err_msg << ",Invalid DoT Host: " << dot_host;
    }
  }

  auto timeout = StringToIntegerU(_timeout);
  if (!timeout.has_value()) {
    err_msg << ",Invalid Connect Timeout: " << _timeout;
  }

  std::string ret = err_msg.str();
  if (ret.empty()) {
    absl::SetFlag(&FLAGS_server_host, server_host);
    absl::SetFlag(&FLAGS_server_sni, server_sni);
    absl::SetFlag(&FLAGS_server_port, server_port.value());
    absl::SetFlag(&FLAGS_username, username);
    absl::SetFlag(&FLAGS_password, password);
    absl::SetFlag(&FLAGS_method, method);
    absl::SetFlag(&FLAGS_local_host, local_host);
    absl::SetFlag(&FLAGS_local_port, local_port.value());
    absl::SetFlag(&FLAGS_doh_url, doh_url);
    absl::SetFlag(&FLAGS_dot_host, dot_host);
    absl::SetFlag(&FLAGS_connect_timeout, timeout.value());
  } else {
    ret = ret.substr(1);
  }
  return ret;
}

std::string ReadConfigFromArgument(std::string_view server_host,
                                   std::string_view server_sni,
                                   std::string_view _server_port,
                                   std::string_view username,
                                   std::string_view password,
                                   std::string_view method_string,
                                   std::string_view local_host,
                                   std::string_view _local_port,
                                   std::string_view doh_url,
                                   std::string_view dot_host,
                                   std::string_view _timeout) {
  std::ostringstream err_msg;

  if (server_host.empty() || server_host.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Server Host: " << server_host;
  }

  if (server_sni.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Server Host: " << server_sni;
  }

  auto server_port = StringToIntegerU(_server_port);
  if (!server_port.has_value() || server_port.value() == 0u || server_port.value() > 65535u) {
    err_msg << ",Invalid Server Port: " << _server_port;
  }

  auto method = to_cipher_method(method_string);
  if (method == CRYPTO_INVALID) {
    err_msg << ",Invalid Cipher: " << method_string;
  }

  if (local_host.empty() || local_host.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Local Host: " << local_host;
  }

  auto local_port = StringToIntegerU(_local_port);
  if (!local_port.has_value() || local_port.value() > 65535u) {
    err_msg << ",Invalid Local Port: " << _local_port;
  }

  if (!doh_url.empty()) {
    GURL url(doh_url);
    if (!url.is_valid() || !url.has_host() || !url.has_scheme() || url.scheme() != "https") {
      err_msg << ",Invalid DoH URL: " << doh_url;
    }
    if (!dot_host.empty()) {
      err_msg << ",Conflicting DoT Host: " << dot_host;
    }
  }

  if (!dot_host.empty()) {
    if (dot_host.size() >= TLSEXT_MAXLEN_host_name) {
      err_msg << ",Invalid DoT Host: " << dot_host;
    }
  }

  auto timeout = StringToIntegerU(_timeout);
  if (!timeout.has_value()) {
    err_msg << ",Invalid Connect Timeout: " << _timeout;
  }

  std::string ret = err_msg.str();
  if (ret.empty()) {
    absl::SetFlag(&FLAGS_server_host, server_host);
    absl::SetFlag(&FLAGS_server_sni, server_sni);
    absl::SetFlag(&FLAGS_server_port, server_port.value());
    absl::SetFlag(&FLAGS_username, username);
    absl::SetFlag(&FLAGS_password, password);
    absl::SetFlag(&FLAGS_method, method);
    absl::SetFlag(&FLAGS_local_host, local_host);
    absl::SetFlag(&FLAGS_local_port, local_port.value());
    absl::SetFlag(&FLAGS_doh_url, doh_url);
    absl::SetFlag(&FLAGS_dot_host, dot_host);
    absl::SetFlag(&FLAGS_connect_timeout, timeout.value());
  } else {
    ret = ret.substr(1);
  }
  return ret;
}

void SetClientUsageMessage(std::string_view exec_path) {
  absl::SetProgramUsageMessage(absl::StrCat("Usage: ", Basename(exec_path), " [options ...]\n", R"(
  -K, --config <file> Read config from a file
  --server_host <host> Remote server on given host
  --server_port <port> Remote server on given port
  --local_host <host> Local proxy server on given host
  --local_port <port> Local proxy server on given port
  --username <username> Server user
  --password <pasword> Server password
  --method <method> Specify encrypt of method to use
  --limit_rate Limits the rate of response transmission to a client. Uint can be (none), k, m.
  --padding_support Enable padding support
  --use_ca_bundle_crt Use builtin ca-bundle.crt instead of system CA store
  --cacert <file> Tells where to use the specified certificate file to verify the peer
  --capath <dir> Tells where to use the specified certificate dir to verify the peer
  --certificate_chain_file <file> Specify Certificate Chain File Path
  -k, --insecure_mode Skip the verification step and proceed without checking
  --tls13_early_data Enable 0RTTI Early Data
  --enable_post_quantum_kyber Enable post-quantum secure TLS key encapsulation mechanism X25519Kyber768, based on a NIST standard (ML-KEM)
)"));
}

void SetServerUsageMessage(std::string_view exec_path) {
  absl::SetProgramUsageMessage(absl::StrCat("Usage: ", Basename(exec_path), " [options ...]\n", R"(
  -K, --config <file> Read config from a file
  --server_host <host> Server on given host
  --server_port <port> Server on given port
  --username <username> Server user
  --password <pasword> Server password
  --method <method> Specify encrypt of method to use
  --limit_rate Limits the rate of response transmission to a client. Uint can be (none), k, m.
  --padding_support Enable padding support
  --use_ca_bundle_crt Use builtin ca-bundle.crt instead of system CA store
  --cacert <file> Tells where to use the specified certificate file to verify the peer
  --capath <dir> Tells where to use the specified certificate dir to verify the peer
  --certificate_chain_file <file> Specify Certificate Chain File Path
  --private_key_file <file> Specify Private Key File Path
  --private_key_password <password> Specify Private Key Password
  --tls13_early_data Enable 0RTTI Early Data
)"));
}

}  // namespace config
