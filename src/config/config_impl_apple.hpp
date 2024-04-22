// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG_IMPL_APPLE
#define H_CONFIG_CONFIG_IMPL_APPLE

#include "config/config_impl.hpp"

#if defined(__APPLE__)

#include <absl/strings/string_view.h>
#include <string>

#include <CoreFoundation/CoreFoundation.h>
#include <base/apple/scoped_cftyperef.h>

namespace config {

class ConfigImplApple : public ConfigImpl {
 public:
  ~ConfigImplApple() override {}

 protected:
  bool OpenImpl(bool dontread) override;

  bool CloseImpl() override;

  bool ReadImpl(const std::string& key, std::string* value) override;
  bool ReadImpl(const std::string& key, bool* value) override;
  bool ReadImpl(const std::string& key, uint32_t* value) override;
  bool ReadImpl(const std::string& key, int32_t* value) override;
  bool ReadImpl(const std::string& key, uint64_t* value) override;
  bool ReadImpl(const std::string& key, int64_t* value) override;

  bool WriteImpl(const std::string& key, std::string_view value) override;
  bool WriteImpl(const std::string& key, bool value) override;
  bool WriteImpl(const std::string& key, uint32_t value) override;
  bool WriteImpl(const std::string& key, int32_t value) override;
  bool WriteImpl(const std::string& key, uint64_t value) override;
  bool WriteImpl(const std::string& key, int64_t value) override;

  bool DeleteImpl(const std::string& key) override;

 private:
  gurl_base::apple::ScopedCFTypeRef<CFDictionaryRef> root_;
  gurl_base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> write_root_;
};

}  // namespace config

#endif  // defined(__APPLE__)
#endif  // H_CONFIG_CONFIG_IMPL_POSIX
