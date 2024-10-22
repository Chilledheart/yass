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

  if (pType_IsClient()) {
    required_fields_loaded &= config_impl->Read("local", &FLAGS_local_host);
    required_fields_loaded &= config_impl->Read("local_port", &FLAGS_local_port);
  }

  /* optional fields */
  config_impl->Read("server_sni", &FLAGS_server_sni);

  config_impl->Read("fast_open", &FLAGS_tcp_fastopen);
  config_impl->Read("fast_open_connect", &FLAGS_tcp_fastopen_connect);

  config_impl->Read("doh_url", &FLAGS_doh_url);
  config_impl->Read("dot_host", &FLAGS_dot_host);
  config_impl->Read("connect_timeout", &FLAGS_connect_timeout);
  config_impl->Read("tcp_nodelay", &FLAGS_tcp_nodelay);
  config_impl->Read("limit_rate", &FLAGS_limit_rate);

  config_impl->Read("tcp_keep_alive", &FLAGS_tcp_keep_alive);
  config_impl->Read("tcp_keep_alive_cnt", &FLAGS_tcp_keep_alive_cnt);
  config_impl->Read("tcp_keep_alive_idle_timeout", &FLAGS_tcp_keep_alive_idle_timeout);
  config_impl->Read("tcp_keep_alive_interval", &FLAGS_tcp_keep_alive_interval);
  /* deprecated option: congestion_algorithm */
  if (config_impl->HasKey<std::string>("congestion_algorithm")) {
    config_impl->Read("congestion_algorithm", &FLAGS_tcp_congestion_algorithm);
  }
  config_impl->Read("tcp_congestion_algorithm", &FLAGS_tcp_congestion_algorithm);

  /* optional tls fields */
  config_impl->Read("cacert", &FLAGS_cacert);
  config_impl->Read("capath", &FLAGS_capath);
  config_impl->Read("certificate_chain_file", &FLAGS_certificate_chain_file);
  if (pType_IsServer()) {
    config_impl->Read("private_key_file", &FLAGS_private_key_file);
    config_impl->Read("private_key_password", &FLAGS_private_key_password, true);
  }
  if (pType_IsClient()) {
    config_impl->Read("insecure_mode", &FLAGS_insecure_mode);
  }
  config_impl->Read("enable_post_quantum_kyber", &FLAGS_enable_post_quantum_kyber);
  config_impl->Read("tls13_early_data", &FLAGS_tls13_early_data);

#if BUILDFLAG(IS_MAC)
  config_impl->Read("ui_display_realtime_status", &FLAGS_ui_display_realtime_status);
#endif

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
  all_fields_written &= config_impl->Write("doh_url", FLAGS_doh_url);
  all_fields_written &= config_impl->Write("dot_host", FLAGS_dot_host);
  all_fields_written &= config_impl->Write("timeout", FLAGS_connect_timeout);
  all_fields_written &= config_impl->Write("connect_timeout", FLAGS_connect_timeout);
  all_fields_written &= config_impl->Write("tcp_nodelay", FLAGS_tcp_nodelay);
  all_fields_written &= config_impl->Write("limit_rate", FLAGS_limit_rate);

  all_fields_written &= config_impl->Write("tcp_keep_alive", FLAGS_tcp_keep_alive);
  all_fields_written &= config_impl->Write("tcp_keep_alive_cnt", FLAGS_tcp_keep_alive_cnt);
  all_fields_written &= config_impl->Write("tcp_keep_alive_idle_timeout", FLAGS_tcp_keep_alive_idle_timeout);
  all_fields_written &= config_impl->Write("tcp_keep_alive_interval", FLAGS_tcp_keep_alive_interval);
  /* remove deprecated option: congestion_algorithm */
  static_cast<void>(config_impl->Delete("congestion_algorithm"));
  all_fields_written &= config_impl->Write("tcp_congestion_algorithm", FLAGS_tcp_congestion_algorithm);

  all_fields_written &= config_impl->Write("cacert", FLAGS_cacert);
  all_fields_written &= config_impl->Write("capath", FLAGS_capath);
  all_fields_written &= config_impl->Write("certificate_chain_file", FLAGS_certificate_chain_file);
  if (pType_IsServer()) {
    all_fields_written &= config_impl->Write("private_key_file", FLAGS_private_key_file);
    all_fields_written &= config_impl->Write("private_key_password", FLAGS_private_key_password);
  }
  if (pType_IsClient()) {
    all_fields_written &= config_impl->Write("insecure_mode", FLAGS_insecure_mode);
    all_fields_written &= config_impl->Write("enable_post_quantum_kyber", FLAGS_enable_post_quantum_kyber);
  }
  all_fields_written &= config_impl->Write("tls13_early_data", FLAGS_tls13_early_data);

#if BUILDFLAG(IS_MAC)
  all_fields_written &= config_impl->Write("ui_display_realtime_status", FLAGS_ui_display_realtime_status);
#endif

  all_fields_written &= config_impl->Close();

  return all_fields_written;
}

std::string ValidateConfig() {
  std::string err;
  std::ostringstream err_msg;

  auto server_host = absl::GetFlag(FLAGS_server_host);
  auto server_sni = absl::GetFlag(FLAGS_server_sni);
  auto server_port = absl::GetFlag(FLAGS_server_port);
  auto username = absl::GetFlag(FLAGS_username);
  auto password = absl::GetFlag(FLAGS_password);
  auto method = absl::GetFlag(FLAGS_method);
  auto local_host = absl::GetFlag(FLAGS_local_host);
  // auto local_port = absl::GetFlag(FLAGS_local_port);
  auto doh_url = absl::GetFlag(FLAGS_doh_url);
  auto dot_host = absl::GetFlag(FLAGS_dot_host);
  // auto limit_rate = absl::GetFlag(FLAGS_limit_rate);
  // auto timeout = absl::GetFlag(FLAGS_connect_timeout);

  // TODO validate other configurations as well

  if (server_host.empty() || server_host.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Server Host: " << server_host;
  }

  if (server_sni.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Server Host: " << server_sni;
  }

  if (server_port.port == 0) {
    err_msg << ",Invalid Server Port: " << server_port.port;
  }

  if (method == CRYPTO_INVALID) {
    err_msg << ",Invalid Cipher: " << to_cipher_method_str(method);
  }

  if (method == CRYPTO_SOCKS4 || method == CRYPTO_SOCKS4A) {
    if (!username.empty() || !password.empty()) {
      err_msg << ",SOCKS4/SOCKSA doesn't support username and passsword";
    }
  }

  if (method == CRYPTO_SOCKS5 || method == CRYPTO_SOCKS5H) {
    if (username.empty() ^ password.empty()) {
      err_msg << ",SOCKS5/SOCKS5H requires both of username and passsword";
    }
  }

  if (CIPHER_METHOD_IS_HTTP(method)) {
    if (username.empty() ^ password.empty()) {
      err_msg << ",HTTP requires both of username and passsword";
    }
  }

  if (local_host.empty() || local_host.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Local Host: " << local_host;
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

  auto ret = err_msg.str();
  if (ret.empty()) {
    return ret;
  } else {
    return ret.substr(1);
  }
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
                                   std::string_view _limit_rate,
                                   std::string_view _timeout) {
  std::string err;
  std::ostringstream err_msg;

  if (server_host.empty() || server_host.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Server Host: " << server_host;
  }

  if (server_sni.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Server Host: " << server_sni;
  }

  PortFlag server_port;
  if (!AbslParseFlag(_server_port, &server_port, &err) || server_port.port == 0u) {
    err_msg << ",Invalid Server Port: " << _server_port;
  }

  if (method == CRYPTO_INVALID) {
    err_msg << ",Invalid Cipher: " << to_cipher_method_str(method);
  }

  if (method == CRYPTO_SOCKS4 || method == CRYPTO_SOCKS4A) {
    if (!username.empty() || !password.empty()) {
      err_msg << ",SOCKS4/SOCKSA doesn't support username and passsword";
    }
  }

  if (method == CRYPTO_SOCKS5 || method == CRYPTO_SOCKS5H) {
    if (username.empty() ^ password.empty()) {
      err_msg << ",SOCKS5/SOCKS5H requires both of username and passsword";
    }
  }

  if (CIPHER_METHOD_IS_HTTP(method)) {
    if (username.empty() ^ password.empty()) {
      err_msg << ",HTTP requires both of username and passsword";
    }
  }

  if (local_host.empty() || local_host.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Local Host: " << local_host;
  }

  PortFlag local_port;
  if (!AbslParseFlag(_local_port, &local_port, &err)) {
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

  RateFlag limit_rate;
  if (!AbslParseFlag(_limit_rate, &limit_rate, &err)) {
    err_msg << ",Invalid Rate Limit: " << _limit_rate;
  }

  int timeout;
  if (!StringToInt(_timeout, &timeout) || timeout < 0) {
    err_msg << ",Invalid Connect Timeout: " << _timeout;
  }

  std::string ret = err_msg.str();
  if (ret.empty()) {
    absl::SetFlag(&FLAGS_server_host, server_host);
    absl::SetFlag(&FLAGS_server_sni, server_sni);
    absl::SetFlag(&FLAGS_server_port, server_port);
    absl::SetFlag(&FLAGS_username, username);
    absl::SetFlag(&FLAGS_password, password);
    absl::SetFlag(&FLAGS_method, method);
    absl::SetFlag(&FLAGS_local_host, local_host);
    absl::SetFlag(&FLAGS_local_port, local_port);
    absl::SetFlag(&FLAGS_doh_url, doh_url);
    absl::SetFlag(&FLAGS_dot_host, dot_host);
    absl::SetFlag(&FLAGS_limit_rate, limit_rate);
    absl::SetFlag(&FLAGS_connect_timeout, timeout);
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
                                   std::string_view _limit_rate,
                                   std::string_view _timeout) {
  std::string err;
  std::ostringstream err_msg;

  if (server_host.empty() || server_host.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Server Host: " << server_host;
  }

  if (server_sni.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Server Host: " << server_sni;
  }

  PortFlag server_port;
  if (!AbslParseFlag(_server_port, &server_port, &err) || server_port.port == 0u) {
    err_msg << ",Invalid Server Port: " << _server_port;
  }

  CipherMethodFlag method;
  if (!AbslParseFlag(method_string, &method, &err)) {
    err_msg << ",Invalid Cipher: " << method_string;
  }

  if (method == CRYPTO_SOCKS4 || method == CRYPTO_SOCKS4A) {
    if (!username.empty() || !password.empty()) {
      err_msg << ",SOCKS4/SOCKSA doesn't support username and passsword";
    }
  }

  if (method == CRYPTO_SOCKS5 || method == CRYPTO_SOCKS5H) {
    if (username.empty() ^ password.empty()) {
      err_msg << ",SOCKS5/SOCKS5H requires both of username and passsword";
    }
  }

  if (CIPHER_METHOD_IS_HTTP(method)) {
    if (username.empty() ^ password.empty()) {
      err_msg << ",HTTP requires both of username and passsword";
    }
  }

  if (local_host.empty() || local_host.size() >= TLSEXT_MAXLEN_host_name) {
    err_msg << ",Invalid Local Host: " << local_host;
  }

  PortFlag local_port;
  if (!AbslParseFlag(_local_port, &local_port, &err)) {
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

  RateFlag limit_rate;
  if (!AbslParseFlag(_limit_rate, &limit_rate, &err)) {
    err_msg << ",Invalid Rate Limit: " << _limit_rate;
  }

  int timeout;
  if (!StringToInt(_timeout, &timeout) || timeout < 0) {
    err_msg << ",Invalid Connect Timeout: " << _timeout;
  }

  std::string ret = err_msg.str();
  if (ret.empty()) {
    absl::SetFlag(&FLAGS_server_host, server_host);
    absl::SetFlag(&FLAGS_server_sni, server_sni);
    absl::SetFlag(&FLAGS_server_port, server_port);
    absl::SetFlag(&FLAGS_username, username);
    absl::SetFlag(&FLAGS_password, password);
    absl::SetFlag(&FLAGS_method, method);
    absl::SetFlag(&FLAGS_local_host, local_host);
    absl::SetFlag(&FLAGS_local_port, local_port);
    absl::SetFlag(&FLAGS_doh_url, doh_url);
    absl::SetFlag(&FLAGS_dot_host, dot_host);
    absl::SetFlag(&FLAGS_limit_rate, limit_rate);
    absl::SetFlag(&FLAGS_connect_timeout, timeout);
  } else {
    ret = ret.substr(1);
  }
  return ret;
}

void SetClientUsageMessage(std::string_view exec_path) {
  absl::SetProgramUsageMessage(absl::StrCat("Usage: ", Basename(exec_path), " [options ...]\n", R"(
  -K, --config <file> Read config from a file
  -t Don't run, just test the configuration file
  -v, --version Print yass version
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
  --certificate_chain_file <file> Use custom certificate chain file to verify server's certificate
  -k, --insecure_mode Skip the verification step and proceed without checking
  --tls13_early_data Enable 0RTTI Early Data
  --enable_post_quantum_kyber Enables post-quantum key-agreements in TLS 1.3 connections. The use_ml_kem flag controls whether ML-KEM or Kyber is used.
  --use_ml_kem Use ML-KEM in TLS 1.3. Causes TLS 1.3 connections to use the ML-KEM standard instead of the Kyber draft standard for post-quantum key-agreement. The enable_post_quantum_kyber flag must be enabled for this to have an effect.
)"));
}

void SetServerUsageMessage(std::string_view exec_path) {
  absl::SetProgramUsageMessage(absl::StrCat("Usage: ", Basename(exec_path), " [options ...]\n", R"(
  -K, --config <file> Read config from a file
  -t Don't run, just test the configuration file
  -v, --version Print yass version
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
  --certificate_chain_file <file> Use custom certificate chain file to verify server's certificate
  --private_key_file <file> Use custom private key file to secure connection between server and client
  --private_key_password <password> Use custom private key password to decrypt server's encrypted private key
  --enable_post_quantum_kyber Enables post-quantum key-agreements in TLS 1.3 connections. The use_ml_kem flag controls whether ML-KEM or Kyber is used.
  --use_ml_kem Use ML-KEM in TLS 1.3. Causes TLS 1.3 connections to use the ML-KEM standard instead of the Kyber draft standard for post-quantum key-agreement. The enable_post_quantum_kyber flag must be enabled for this to have an effect.
)"));
}

}  // namespace config
