// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#include "config/config_impl_apple.hpp"

#ifdef __APPLE__

#include <unistd.h>

#include <absl/flags/declare.h>
#include <sys/stat.h>
#include <iostream>
#include <memory>

#include "core/utils.hpp"

#include <base/apple/foundation_util.h>

ABSL_DECLARE_FLAG(std::string, configfile);

using namespace gurl_base::apple;

// Because a suite manages the defaults of a specified app group, a suite name
// must be distinct from your app’s main bundle identifier.
static const char* kYassSuiteName = "it.gui.yass.suite";
static const char* kYassKeyName = "YASSConfiguration";

namespace config {

bool ConfigImplApple::OpenImpl(bool dontread) {
  dontread_ = dontread;

  ScopedCFTypeRef<CFMutableDictionaryRef> mutable_root(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

  // Overwrite all fields from UserDefaults
  NSUserDefaults* defaultsRoot = [[NSUserDefaults alloc] initWithSuiteName:@(kYassSuiteName)];

  NSDictionary* root = [defaultsRoot dictionaryForKey:@(kYassKeyName)];

  if (root) {
    for (NSString* key in root) {
      id value = root[key];
      ScopedCFTypeRef<CFStringRef> cf_key((CFStringRef)CFBridgingRetain(key));
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

bool ConfigImplApple::HasKeyStringImpl(const std::string& key) {
  CFStringRef obj;
  if (CFDictionaryGetValueIfPresent(root_, SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFStringGetTypeID()) {
    return true;
  }
  return false;
}

bool ConfigImplApple::HasKeyBoolImpl(const std::string& key) {
  CFBooleanRef obj;
  if (CFDictionaryGetValueIfPresent(root_, SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFBooleanGetTypeID()) {
    return true;
  }
  return false;
}

bool ConfigImplApple::HasKeyUint32Impl(const std::string& key) {
  return HasKeyInt32Impl(key);
}

bool ConfigImplApple::HasKeyUint64Impl(const std::string& key) {
  return HasKeyInt64Impl(key);
}

bool ConfigImplApple::HasKeyInt32Impl(const std::string& key) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(root_, SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() &&
      (CFNumberGetType(obj) == kCFNumberSInt32Type || CFNumberGetType(obj) == kCFNumberSInt16Type ||
       CFNumberGetType(obj) == kCFNumberSInt8Type)) {
    return true;
  }
  return false;
}

bool ConfigImplApple::HasKeyInt64Impl(const std::string& key) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(root_, SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() &&
      (CFNumberGetType(obj) == kCFNumberSInt64Type || CFNumberGetType(obj) == kCFNumberSInt32Type ||
       CFNumberGetType(obj) == kCFNumberSInt16Type || CFNumberGetType(obj) == kCFNumberSInt8Type)) {
    return true;
  }
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, std::string* value) {
  CFStringRef obj;
  if (CFDictionaryGetValueIfPresent(root_, SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFStringGetTypeID()) {
    *value = SysCFStringRefToUTF8(obj);
    return true;
  }
  std::cerr << "bad field: " << key << std::endl;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, bool* value) {
  CFBooleanRef obj;
  if (CFDictionaryGetValueIfPresent(root_, SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFBooleanGetTypeID()) {
    *value = CFBooleanGetValue(obj);
    return true;
  }
  std::cerr << "bad field: " << key << std::endl;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, uint32_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(root_, SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() && CFNumberGetValue(obj, kCFNumberSInt32Type, value)) {
    return true;
  }
  std::cerr << "bad field: " << key << std::endl;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, int32_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(root_, SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() && CFNumberGetValue(obj, kCFNumberSInt32Type, value)) {
    return true;
  }
  std::cerr << "bad field: " << key << std::endl;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, uint64_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(root_, SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() && CFNumberGetValue(obj, kCFNumberSInt64Type, value)) {
    return true;
  }
  std::cerr << "bad field: " << key << std::endl;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, int64_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(root_, SysUTF8ToCFStringRef(key).get(), (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() && CFNumberGetValue(obj, kCFNumberSInt64Type, value)) {
    return true;
  }
  std::cerr << "bad field: " << key << std::endl;
  return false;
}

bool ConfigImplApple::WriteImpl(const std::string& key, std::string_view value) {
  ScopedCFTypeRef<CFStringRef> obj(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(value.data()), value.size(), kCFStringEncodingUTF8, FALSE));
  CFDictionarySetValue(write_root_, SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, bool value) {
  ScopedCFTypeRef<CFBooleanRef> obj(value ? kCFBooleanTrue : kCFBooleanFalse);
  CFDictionarySetValue(write_root_, SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, uint32_t value) {
  ScopedCFTypeRef<CFNumberRef> obj(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value));
  CFDictionarySetValue(write_root_, SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, int32_t value) {
  ScopedCFTypeRef<CFNumberRef> obj(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value));
  CFDictionarySetValue(write_root_, SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, uint64_t value) {
  ScopedCFTypeRef<CFNumberRef> obj(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value));
  CFDictionarySetValue(write_root_, SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, int64_t value) {
  ScopedCFTypeRef<CFNumberRef> obj(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value));
  CFDictionarySetValue(write_root_, SysUTF8ToCFStringRef(key).get(), obj);
  return true;
}

bool ConfigImplApple::DeleteImpl(const std::string& key) {
  if (CFDictionaryContainsKey(write_root_, SysUTF8ToCFStringRef(key).get())) {
    CFDictionaryRemoveValue(write_root_, SysUTF8ToCFStringRef(key).get());
    return true;
  }
  return false;
}

}  // namespace config

#endif  // __APPLE__
