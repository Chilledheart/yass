// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */
#include "config/config_impl.hpp"

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <stdint.h>
#include <cassert>
#include <iostream>

#include "config/config_impl_apple.hpp"
#include "config/config_impl_local.hpp"
#include "config/config_impl_windows.hpp"
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
    std::cerr << "using option from file: " << std::endl;
    auto config = std::make_unique<ConfigImplLocal>(g_configfile);
    config->SetEnforceRead();
    return config;
  }
#ifdef _WIN32
  std::cerr << "using option from registry" << std::endl;
  return std::make_unique<ConfigImplWindows>();
#elif defined(__APPLE__)
  std::cerr << "using option from defaults database" << std::endl;
  return std::make_unique<ConfigImplApple>();
#elif defined(__ANDROID__)
  std::string configfile = absl::StrCat(a_data_dir, "/", "config.json");
  std::cerr << "using option from file: " << configfile << std::endl;
  return std::make_unique<ConfigImplLocal>(configfile);
#elif defined(__OHOS__)
  std::string configfile = absl::StrCat(h_data_dir, "/", "config.json");
  std::cerr << "using option from file: " << configfile << std::endl;
  return std::make_unique<ConfigImplLocal>(configfile);
#else
  const char* const configfile = "~/.yass/config.json";
  std::cerr << "using option from file: " << configfile << std::endl;
  return std::make_unique<ConfigImplLocal>(configfile);
#endif
}

bool ConfigImpl::Open(bool dontread) {
  bool ret = OpenImpl(dontread);
  if (!ret) {
    std::cerr << "failed to open config" << std::endl;
  } else {
    std::cerr << "opened config" << std::endl;
  }
  return ret;
}

bool ConfigImpl::Close() {
  bool ret = CloseImpl();
  if (!ret) {
    std::cerr << "failed to close/sync config" << std::endl;
  } else {
    std::cerr << "closed config" << std::endl;
  }
  return ret;
}

template <typename T>
bool ConfigImpl::Read(const std::string& key, absl::Flag<T>* value) {
  // Use an int instead of a bool to guarantee that a non-zero value has
  // a bit set.

  alignas(T) alignas(8) T real_value;
  if (!ReadImpl(key, &real_value)) {
    std::cerr << "failed to load option " << key << std::endl;
    return false;
  }
  absl::SetFlag(value, real_value);
  std::cerr << "loaded option " << key << ": " << real_value << std::endl;
  return true;
}

template bool ConfigImpl::Read(const std::string& key, absl::Flag<std::string>* value);

template <>
bool ConfigImpl::Read(const std::string& key, absl::Flag<PortFlag>* value) {
  alignas(std::string) alignas(8) int32_t real_value;
  if (!ReadImpl(key, &real_value)) {
    std::cerr << "failed to load option " << key << std::endl;
    return false;
  }
  if (real_value < 0 || real_value > UINT16_MAX) {
    std::cerr << "invalid value for key: " << key << " value: " << real_value << std::endl;
    return false;
  }
  PortFlag p(static_cast<uint16_t>(real_value));
  absl::SetFlag(value, p);
  std::cerr << "loaded option " << key << ": " << real_value << std::endl;
  return true;
}

template <>
bool ConfigImpl::Read(const std::string& key, absl::Flag<CipherMethodFlag>* value) {
  alignas(std::string) alignas(8) std::string real_value;
  if (!ReadImpl(key, &real_value)) {
    std::cerr << "failed to load option " << key << std::endl;
    return false;
  }
  auto cipher_method = to_cipher_method(real_value);
  if (cipher_method == CRYPTO_INVALID) {
    std::cerr << "invalid value for key: " << key << " value: " << real_value << std::endl;
    return false;
  }
  CipherMethodFlag method(cipher_method);
  absl::SetFlag(value, method);
  std::cerr << "loaded option " << key << ": " << real_value << std::endl;
  return true;
}

template <>
bool ConfigImpl::Read(const std::string& key, absl::Flag<bool>* value) {
  // Use an int instead of a bool to guarantee that a non-zero value has
  // a bit set.

  alignas(bool) alignas(8) bool real_value;
  if (!ReadImpl(key, &real_value)) {
    std::cerr << "failed to load option " << key << std::endl;
    return false;
  }
  absl::SetFlag(value, real_value);
  std::cerr << "loaded option " << key << ": " << std::boolalpha << real_value << std::noboolalpha << std::endl;
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
    std::cerr << "failed to saved option " << key << ": " << real_value << std::endl;
    return false;
  }
  std::cerr << "saved option " << key << ": " << real_value << std::endl;
  return true;
}

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<std::string>& value);

template <>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<PortFlag>& value) {
  int32_t real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    std::cerr << "failed to saved option " << key << ": " << real_value << std::endl;
    return false;
  }
  std::cerr << "saved option " << key << ": " << real_value << std::endl;
  return true;
}

template <>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<CipherMethodFlag>& value) {
  auto cipher_method = absl::GetFlag(value).method;
  assert(is_valid_cipher_method(cipher_method) && "Invalid cipher_method");
  auto real_value = to_cipher_method_str(cipher_method);
  if (!WriteImpl(key, real_value)) {
    std::cerr << "failed to saved option " << key << ": " << real_value << std::endl;
    return false;
  }
  std::cerr << "saved option " << key << ": " << real_value << std::endl;
  return true;
}

template <>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<bool>& value) {
  alignas(bool) alignas(8) bool real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    std::cerr << "failed to saved option " << key << ": " << std::boolalpha << real_value << std::noboolalpha
              << std::endl;
    return false;
  }
  std::cerr << "saved option " << key << ": " << std::boolalpha << real_value << std::noboolalpha << std::endl;
  return true;
}

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<uint32_t>& value);

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<int32_t>& value);

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<uint64_t>& value);

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<int64_t>& value);

bool ConfigImpl::Delete(const std::string& key) {
  if (DeleteImpl(key)) {
    std::cerr << "deleted option " << key << std::endl;
    return true;
  }
  return false;
}

}  // namespace config
