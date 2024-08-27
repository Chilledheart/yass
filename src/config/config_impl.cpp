// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */
#include "config/config_impl.hpp"

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <stdint.h>
#include <cassert>
#include <iostream>

#include "config/config_core.hpp"
#include "config/config_export.hpp"
#include "config/config_impl_apple.hpp"
#include "config/config_impl_local.hpp"
#include "config/config_impl_windows.hpp"
#include "crypto/crypter_export.hpp"

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace config {

namespace {

template <typename T>
std::string to_masked_string(T value, bool is_masked);

template <>
std::string to_masked_string(std::string value, bool is_masked) {
  if (value.empty()) {
    value = "(nil)"s;
  }
  std::string mask_value = value;
  if (is_masked) {
    mask_value = std::string(mask_value.size(), '*');
  }
  return mask_value;
}

template <>
std::string to_masked_string(std::string_view value, bool is_masked) {
  if (value.empty()) {
    value = "(nil)"sv;
  }
  std::string mask_value = std::string(value);
  if (is_masked) {
    mask_value = std::string(mask_value.size(), '*');
  }
  return mask_value;
}

template <>
std::string to_masked_string(bool value, bool is_masked) {
  std::string mask_value = value ? "true"s : "false"s;
  if (is_masked) {
    mask_value = std::string(mask_value.size(), '*');
  }
  return mask_value;
}

template <typename T>
std::string to_masked_string(T value, bool is_masked) {
  std::string mask_value = std::to_string(value);
  if (is_masked) {
    mask_value = std::string(mask_value.size(), '*');
  }
  return mask_value;
}

}  // namespace

std::string g_configfile;

ConfigImpl::~ConfigImpl() = default;

std::unique_ptr<ConfigImpl> ConfigImpl::Create() {
  if (!g_configfile.empty()) {
    std::cerr << "using option from file: " << g_configfile << std::endl;
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

template <>
bool ConfigImpl::HasKey<std::string>(const std::string& key) {
  return HasKeyStringImpl(key);
}

template <>
bool ConfigImpl::HasKey<bool>(const std::string& key) {
  return HasKeyBoolImpl(key);
}

template <>
bool ConfigImpl::HasKey<uint32_t>(const std::string& key) {
  return HasKeyUint32Impl(key);
}

template <>
bool ConfigImpl::HasKey<uint64_t>(const std::string& key) {
  return HasKeyUint64Impl(key);
}

template <>
bool ConfigImpl::HasKey<int32_t>(const std::string& key) {
  return HasKeyInt32Impl(key);
}

template <>
bool ConfigImpl::HasKey<int64_t>(const std::string& key) {
  return HasKeyInt64Impl(key);
}

template <typename T>
bool ConfigImpl::Read(const std::string& key, absl::Flag<T>* value, bool is_masked) {
  // Use an int instead of a bool to guarantee that a non-zero value has
  // a bit set.

  alignas(T) alignas(8) T real_value;
  if (!ReadImpl(key, &real_value)) {
    std::cerr << "failed to load option " << key << std::endl;
    return false;
  }
  absl::SetFlag(value, real_value);
  std::cerr << "loaded option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
  return true;
}

template bool ConfigImpl::Read(const std::string& key, absl::Flag<std::string>* value, bool is_masked);

template <>
bool ConfigImpl::Read(const std::string& key, absl::Flag<PortFlag>* value, bool is_masked) {
  alignas(std::string) alignas(8) int32_t real_value;
  if (!ReadImpl(key, &real_value)) {
    std::cerr << "failed to load option " << key << std::endl;
    return false;
  }
  if (real_value < 0 || real_value > UINT16_MAX) {
    std::cerr << "invalid value for key: " << key << " value: " << to_masked_string(real_value, is_masked) << std::endl;
    return false;
  }
  PortFlag p(static_cast<uint16_t>(real_value));
  absl::SetFlag(value, p);
  std::cerr << "loaded option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
  return true;
}

template <>
bool ConfigImpl::Read(const std::string& key, absl::Flag<CipherMethodFlag>* value, bool is_masked) {
  alignas(std::string) alignas(8) std::string real_value;
  if (!ReadImpl(key, &real_value)) {
    std::cerr << "failed to load option " << key << std::endl;
    return false;
  }
  std::string err;
  CipherMethodFlag method;
  if (!AbslParseFlag(real_value, &method, &err)) {
    std::cerr << "invalid value for key: " << key << " value: " << to_masked_string(real_value, is_masked) << std::endl;
    return false;
  }
  absl::SetFlag(value, method);
  std::cerr << "loaded option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
  return true;
}

template <>
bool ConfigImpl::Read(const std::string& key, absl::Flag<RateFlag>* value, bool is_masked) {
  alignas(std::string) alignas(8) std::string real_value;
  if (!ReadImpl(key, &real_value)) {
    std::cerr << "failed to load option " << key << std::endl;
    return false;
  }
  std::string err;
  RateFlag limit_rate;
  if (!AbslParseFlag(real_value, &limit_rate, &err)) {
    std::cerr << "invalid value for key: " << key << " value: " << to_masked_string(real_value, is_masked) << std::endl;
    return false;
  }
  absl::SetFlag(value, limit_rate);
  std::cerr << "loaded option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
  return true;
}

template <>
bool ConfigImpl::Read(const std::string& key, absl::Flag<bool>* value, bool is_masked) {
  // Use an int instead of a bool to guarantee that a non-zero value has
  // a bit set.

  alignas(bool) alignas(8) bool real_value;
  if (!ReadImpl(key, &real_value)) {
    std::cerr << "failed to load option " << key << std::endl;
    return false;
  }
  absl::SetFlag(value, real_value);
  std::cerr << "loaded option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
  return true;
}

template bool ConfigImpl::Read(const std::string& key, absl::Flag<uint32_t>* value, bool is_masked);

template bool ConfigImpl::Read(const std::string& key, absl::Flag<int32_t>* value, bool is_masked);

template bool ConfigImpl::Read(const std::string& key, absl::Flag<uint64_t>* value, bool is_masked);

template bool ConfigImpl::Read(const std::string& key, absl::Flag<int64_t>* value, bool is_masked);

template <typename T>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<T>& value, bool is_masked) {
  alignas(T) alignas(8) T real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    std::cerr << "failed to saved option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
    return false;
  }
  std::cerr << "saved option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
  return true;
}

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<std::string>& value, bool is_masked);

template <>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<PortFlag>& value, bool is_masked) {
  int32_t real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    std::cerr << "failed to saved option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
    return false;
  }
  std::cerr << "saved option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
  return true;
}

template <>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<CipherMethodFlag>& value, bool is_masked) {
  std::string_view real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    std::cerr << "failed to saved option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
    return false;
  }
  std::cerr << "saved option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
  return true;
}

template <>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<RateFlag>& value, bool is_masked) {
  std::string real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    std::cerr << "failed to saved option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
    return false;
  }
  std::cerr << "saved option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
  return true;
}

template <>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<bool>& value, bool is_masked) {
  alignas(bool) alignas(8) bool real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    std::cerr << "failed to saved option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
    return false;
  }
  std::cerr << "saved option " << key << ": " << to_masked_string(real_value, is_masked) << std::endl;
  return true;
}

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<uint32_t>& value, bool is_masked);

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<int32_t>& value, bool is_masked);

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<uint64_t>& value, bool is_masked);

template bool ConfigImpl::Write(const std::string& key, const absl::Flag<int64_t>& value, bool is_masked);

bool ConfigImpl::Delete(const std::string& key) {
  if (DeleteImpl(key)) {
    std::cerr << "deleted option " << key << std::endl;
    return true;
  }
  return false;
}

}  // namespace config
