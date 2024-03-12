// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */
#include "config/config_impl.hpp"

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <stdint.h>

#include "config/config_impl_apple.hpp"
#include "config/config_impl_local.hpp"
#include "config/config_impl_windows.hpp"
#include "core/logging.hpp"
#include "crypto/crypter_export.hpp"

struct PortFlag {
  explicit PortFlag(uint16_t p) : port(p) {}
  operator uint16_t() const { return port; }
  uint16_t port;
};

struct CipherMethodFlag {
  explicit CipherMethodFlag(cipher_method m) : method(m) {}
  cipher_method method;
};

namespace config {

std::string g_configfile;

ConfigImpl::~ConfigImpl() = default;

std::unique_ptr<ConfigImpl> ConfigImpl::Create() {
  if (!g_configfile.empty()) {
    fprintf(stderr, "using option from file: %s\n", g_configfile.c_str());
    fflush(stderr);
    auto config = std::make_unique<ConfigImplLocal>(g_configfile);
    config->SetEnforceRead();
    return config;
  }
#ifdef _WIN32
  fprintf(stderr, "using option from registry\n");
  fflush(stderr);
  return std::make_unique<ConfigImplWindows>();
#elif defined(__APPLE__)
  fprintf(stderr, "using option from defaults database\n");
  fflush(stderr);
  return std::make_unique<ConfigImplApple>();
#elif defined(__ANDROID__)
  std::string configfile = absl::StrCat(a_data_dir, "/", "config.json");
  fprintf(stderr, "using option from file: %s\n", configfile.c_str());
  fflush(stderr);
  return std::make_unique<ConfigImplLocal>(configfile);
#elif defined(__OHOS__)
  std::string configfile = absl::StrCat(h_data_dir, "/", "config.json");
  fprintf(stderr, "using option from file: %s\n", configfile.c_str());
  fflush(stderr);
  return std::make_unique<ConfigImplLocal>(configfile);
#else
  const char* configfile = "~/.yass/config.json";
  fprintf(stderr, "using option from file: %s\n", configfile);
  fflush(stderr);
  return std::make_unique<ConfigImplLocal>(configfile);
#endif
}

bool ConfigImpl::Open(bool dontread) {
  bool ret = OpenImpl(dontread);
  if (!ret) {
    LOG(ERROR) << "failed to open config";
  } else {
    VLOG(2) << "opened config";
  }
  return ret;
}

bool ConfigImpl::Close() {
  bool ret = CloseImpl();
  if (!ret) {
    LOG(ERROR) << "failed to close/sync config";
  } else {
    VLOG(2) << "closed config";
  }
  return ret;
}

template <typename T>
bool ConfigImpl::Read(const std::string& key, absl::Flag<T>* value) {
  // Use an int instead of a bool to guarantee that a non-zero value has
  // a bit set.

  alignas(T) alignas(8) T real_value;
  if (!ReadImpl(key, &real_value)) {
    LOG(WARNING) << "failed to load option " << key;
    return false;
  }
  absl::SetFlag(value, real_value);
  VLOG(1) << "loaded option " << key << ": " << real_value;
  return true;
}

template bool ConfigImpl::Read(const std::string& key, absl::Flag<std::string>* value);

template <>
bool ConfigImpl::Read(const std::string& key, absl::Flag<PortFlag>* value) {
  alignas(std::string) alignas(8) uint64_t real_value;
  if (!ReadImpl(key, &real_value)) {
    LOG(WARNING) << "failed to load option " << key;
    return false;
  }
  if (real_value > UINT16_MAX) {
    LOG(WARNING) << "invalid value for key: " << key << " value: " << real_value;
    return false;
  }
  PortFlag p(static_cast<uint16_t>(real_value));
  absl::SetFlag(value, p);
  VLOG(1) << "loaded option " << key << ": " << real_value;
  return true;
}

template <>
bool ConfigImpl::Read(const std::string& key, absl::Flag<CipherMethodFlag>* value) {
  alignas(std::string) alignas(8) std::string real_value;
  if (!ReadImpl(key, &real_value)) {
    LOG(WARNING) << "failed to load option " << key;
    return false;
  }
  auto cipher_method = to_cipher_method(real_value);
  if (cipher_method == CRYPTO_INVALID) {
    LOG(WARNING) << "invalid value for key: " << key << " value: " << real_value;
    return false;
  }
  CipherMethodFlag method(cipher_method);
  absl::SetFlag(value, method);
  VLOG(1) << "loaded option " << key << ": " << real_value;
  return true;
}

template <>
bool ConfigImpl::Read(const std::string& key, absl::Flag<bool>* value) {
  // Use an int instead of a bool to guarantee that a non-zero value has
  // a bit set.

  alignas(bool) alignas(8) bool real_value;
  if (!ReadImpl(key, &real_value)) {
    LOG(WARNING) << "failed to load option " << key;
    return false;
  }
  absl::SetFlag(value, real_value);
  VLOG(1) << "loaded option " << key << ": " << std::boolalpha << real_value;
  return true;
}

template bool ConfigImpl::Read(const std::string& key, absl::Flag<uint32_t>* value);

template bool ConfigImpl::Read(const std::string& key, absl::Flag<int32_t>* value);

template bool ConfigImpl::Read(const std::string& key, absl::Flag<uint64_t>* value);

template bool ConfigImpl::Read(const std::string& key, absl::Flag<int64_t>* value);

template <typename T>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<T>& value) {
  alignas(T) alignas(8) T real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    LOG(WARNING) << "failed to saved option " << key << ": " << real_value;
    return false;
  }
  VLOG(1) << "saved option " << key << ": " << real_value;
  return true;
}

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<std::string>& value);

template <>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<PortFlag>& value) {
  uint64_t real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    LOG(WARNING) << "failed to saved option " << key << ": " << real_value;
    return false;
  }
  VLOG(1) << "saved option " << key << ": " << real_value;
  return true;
}

template <>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<CipherMethodFlag>& value) {
  auto cipher_method = absl::GetFlag(value).method;
  DCHECK(is_valid_cipher_method(cipher_method));
  auto real_value = to_cipher_method_str(cipher_method);
  if (!WriteImpl(key, real_value)) {
    LOG(WARNING) << "failed to saved option " << key << ": " << real_value;
    return false;
  }
  VLOG(1) << "saved option " << key << ": " << real_value;
  return true;
}

template <>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<bool>& value) {
  alignas(bool) alignas(8) bool real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    LOG(WARNING) << "failed to saved option " << key << ": " << std::boolalpha << real_value;
    return false;
  }
  VLOG(1) << "saved option " << key << ": " << std::boolalpha << real_value;
  return true;
}

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<uint32_t>& value);

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<int32_t>& value);

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<uint64_t>& value);

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<int64_t>& value);

bool ConfigImpl::Delete(const std::string& key) {
  if (DeleteImpl(key)) {
    VLOG(1) << "deleted option " << key;
    return true;
  }
  return false;
}

}  // namespace config
