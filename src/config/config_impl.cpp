// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "config/config_impl.hpp"

#include <absl/flags/flag.h>
#include <stdint.h>

#include "config/config_impl_apple.hpp"
#include "config/config_impl_posix.hpp"
#include "config/config_impl_windows.hpp"
#include "core/logging.hpp"

namespace config {

ConfigImpl::~ConfigImpl() = default;

std::unique_ptr<ConfigImpl> ConfigImpl::Create() {
#ifdef _WIN32
  return std::make_unique<ConfigImplWindows>();
#elif defined(__APPLE__)
  return std::make_unique<ConfigImplApple>();
#else
  return std::make_unique<ConfigImplPosix>();
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
  alignas(T) alignas(8) T real_value = absl::GetFlag(value);
  if (!WriteImpl(key, real_value)) {
    LOG(WARNING) << "failed to saved option " << key << ": " << real_value;
    return false;
  }
  VLOG(1) << "saved option " << key << ": " << real_value;
  return true;
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

}  // namespace config
