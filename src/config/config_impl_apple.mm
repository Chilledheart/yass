// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */

#include "config/config_impl_apple.hpp"

#ifdef __APPLE__

#include <unistd.h>

#include <absl/flags/declare.h>
#include <sys/stat.h>
#include <memory>

#include "core/logging.hpp"
#include "core/utils.hpp"

#include <base/apple/foundation_util.h>

ABSL_DECLARE_FLAG(std::string, configfile);

// Because a suite manages the defaults of a specified app group, a suite name
// must be distinct from your app’s main bundle identifier.
static const char* kYassSuiteName = "it.gui.yass.suite";
static const char* kYassKeyName = "YASSConfiguration";

namespace config {

bool ConfigImplApple::OpenImpl(bool dontread) {
  dontread_ = dontread;

  gurl_base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> mutable_root(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

  // Overwrite all fields from UserDefaults
  NSUserDefaults* defaultsRoot = [[NSUserDefaults alloc] initWithSuiteName:@(kYassSuiteName)];

  NSDictionary* root = [defaultsRoot dictionaryForKey:@(kYassKeyName)];

  if (root) {
    for (NSString* key in root) {
      id value = root[key];
      gurl_base::apple::ScopedCFTypeRef<CFStringRef> cf_key((CFStringRef)CFBridgingRetain(key));
      CFDictionarySetValue(mutable_root, cf_key, (void*)value);
    }
  }

  if (!dontread) {
    root_.reset(CFDictionaryCreateCopy(kCFAllocatorDefault, mutable_root));
  } else {
    write_root_.reset(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, mutable_root));
  }

  return true;
}

bool ConfigImplApple::CloseImpl() {
  if (!dontread_) {
    root_.reset();
    return true;
  }

  NSUserDefaults* userDefaults = [[NSUserDefaults alloc] initWithSuiteName:@(kYassSuiteName)];
  [userDefaults setObject:CFBridgingRelease(CFRetain(write_root_)) forKey:@(kYassKeyName)];

  write_root_.reset();

  return true;
}

bool ConfigImplApple::ReadImpl(const std::string& key, std::string* value) {
  CFStringRef obj;
  if (CFDictionaryGetValueIfPresent(root_, gurl_base::SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFStringGetTypeID()) {
    *value = gurl_base::SysCFStringRefToUTF8(obj);
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, bool* value) {
  CFBooleanRef obj;
  if (CFDictionaryGetValueIfPresent(root_, gurl_base::SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFBooleanGetTypeID()) {
    *value = CFBooleanGetValue(obj);
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, uint32_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(root_, gurl_base::SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() && CFNumberGetValue(obj, kCFNumberSInt32Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, int32_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(root_, gurl_base::SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() && CFNumberGetValue(obj, kCFNumberSInt32Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, uint64_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(root_, gurl_base::SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() && CFNumberGetValue(obj, kCFNumberSInt64Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, int64_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(root_, gurl_base::SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() && CFNumberGetValue(obj, kCFNumberSInt64Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::WriteImpl(const std::string& key, std::string_view value) {
  gurl_base::apple::ScopedCFTypeRef<CFStringRef> obj(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(value.data()), value.size(), kCFStringEncodingUTF8, FALSE));
  CFDictionarySetValue(write_root_, gurl_base::SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, bool value) {
  gurl_base::apple::ScopedCFTypeRef<CFBooleanRef> obj(value ? kCFBooleanTrue : kCFBooleanFalse);
  CFDictionarySetValue(write_root_, gurl_base::SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, uint32_t value) {
  gurl_base::apple::ScopedCFTypeRef<CFNumberRef> obj(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value));
  CFDictionarySetValue(write_root_, gurl_base::SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, int32_t value) {
  gurl_base::apple::ScopedCFTypeRef<CFNumberRef> obj(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value));
  CFDictionarySetValue(write_root_, gurl_base::SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, uint64_t value) {
  gurl_base::apple::ScopedCFTypeRef<CFNumberRef> obj(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value));
  CFDictionarySetValue(write_root_, gurl_base::SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, int64_t value) {
  gurl_base::apple::ScopedCFTypeRef<CFNumberRef> obj(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value));
  CFDictionarySetValue(write_root_, gurl_base::SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::DeleteImpl(const std::string& key) {
  if (CFDictionaryGetValueIfPresent(write_root_, gurl_base::SysUTF8ToCFStringRef(key).get(), nullptr)) {
    CFDictionaryRemoveValue(write_root_, gurl_base::SysUTF8ToCFStringRef(key).get());
    return true;
  }
  return false;
}

}  // namespace config

#endif  // __APPLE__
