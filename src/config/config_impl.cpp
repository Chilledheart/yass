// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "config/config_impl.hpp"

#include <absl/flags/flag.h>
#include <stdint.h>

#include "config/config_impl_posix.hpp"
#include "config/config_impl_windows.hpp"

namespace config {

ConfigImpl::~ConfigImpl() = default;

std::unique_ptr<ConfigImpl> ConfigImpl::Create() {
#ifdef _WIN32
  return std::make_unique<ConfigImplWindows>();
#else
  return std::make_unique<ConfigImplPosix>();
#endif
}

template <typename T>
bool ConfigImpl::Read(const std::string& key, absl::Flag<T>* value) {
  // Use an int instead of a bool to guarantee that a non-zero value has
  // a bit set.

  alignas(T) alignas(8) T real_value;
  if (!Read(key, &real_value)) {
      return false;
  }
  absl::SetFlag(value, real_value);
  return true;
}

template bool ConfigImpl::Read(const std::string& key,
                               absl::Flag<std::string>* value);

template bool ConfigImpl::Read(const std::string& key, absl::Flag<bool>* value);

template bool ConfigImpl::Read(const std::string& key,
                               absl::Flag<uint32_t>* value);

template bool ConfigImpl::Read(const std::string& key,
                               absl::Flag<int32_t>* value);

template bool ConfigImpl::Read(const std::string& key,
                               absl::Flag<uint64_t>* value);

template bool ConfigImpl::Read(const std::string& key,
                               absl::Flag<int64_t>* value);

template <typename T>
bool ConfigImpl::Write(const std::string& key, const absl::Flag<T>& value) {
  return Write(key, absl::GetFlag(value));
}

template bool ConfigImpl::Write(const std::string& key,
                                const absl::Flag<std::string>& value);

template bool ConfigImpl::Write(const std::string& key,
                                const absl::Flag<bool>& value);

template bool ConfigImpl::Write(const std::string& key,
                                const absl::Flag<uint32_t>& value);

template bool ConfigImpl::Write(const std::string& key,
                                const absl::Flag<int32_t>& value);

template bool ConfigImpl::Write(const std::string& key,
                                const absl::Flag<uint64_t>& value);

template bool ConfigImpl::Write(const std::string& key,
                                const absl::Flag<int64_t>& value);

} // namespace config
