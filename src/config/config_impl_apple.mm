// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "config/config_impl_apple.hpp"

#ifdef __APPLE__

#include <unistd.h>

#include <absl/flags/declare.h>
#include <sys/stat.h>
#include <memory>

#include "core/foundation_util.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"

ABSL_DECLARE_FLAG(std::string, configfile);

// Because a suite manages the defaults of a specified app group, a suite name
// must be distinct from your app’s main bundle identifier.
static const char* kYassSuiteName = "it.gui.yass.suite";
static const char* kYassKeyName = "YASSConfiguration";

namespace {

std::string ExpandUser(const std::string& file_path) {
  std::string real_path = file_path;

  if (!real_path.empty() && real_path[0] == '~') {
    std::string home = getenv("HOME");
    return home + "/" + real_path.substr(2);
  }

  return real_path;
}

bool IsFile(const std::string& path) {
  struct stat s;
  return stat(path.c_str(), &s) == 0;
}

bool LoadConfigFromLegacyConfig(const std::string& path,
                                CFMutableDictionaryRef mutable_root) {
  NSError* error = nil;
  NSData* data;

  if (!IsFile(path)) {
    VLOG(2) << "legacy configure file not exists";
    return false;
  }

  data = [NSData dataWithContentsOfFile:@(path.c_str())
                                options:NSDataReadingUncached
                                  error:&error];
  if (!data || error) {
    VLOG(1) << "legacy configure file failed to read: " << path
            << " error: " << error;
    return false;
  }
  error = nil;
  ScopedCFTypeRef<CFDictionaryRef> root;
  NSDictionary* dictionary =
      [NSJSONSerialization JSONObjectWithData:data
                                      options:NSJSONReadingMutableContainers
                                        error:&error];
  root.reset((CFDictionaryRef)CFBridgingRetain(dictionary));
  if (!root || error) {
    VLOG(1) << "legacy configure file failed to parse: " << error
            << " content: \"" << data << "\"";
    return false;
  }

  [CFBridgingRelease(CFRetain(mutable_root)) addEntriesFromDictionary:dictionary];
  return true;
}

}  // anonymous namespace

namespace config {

bool ConfigImplApple::OpenImpl(bool dontread) {
  dontread_ = dontread;

  path_ = ExpandUser(absl::GetFlag(FLAGS_configfile));

  if (!dontread) {
    ScopedCFTypeRef<CFMutableDictionaryRef> mutable_root(
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));

    if (LoadConfigFromLegacyConfig(path_, mutable_root)) {
      LOG(WARNING) << "loaded from legacy config file: " << path_;
    }

    // Overwrite all fields from UserDefaults
    NSUserDefaults* defaults_root =
        [[NSUserDefaults alloc] initWithSuiteName:@(kYassSuiteName)];

    NSDictionary *root = [defaults_root dictionaryForKey:@(kYassKeyName)];

    if (root) {
      for (NSString* key in root) {
        id value = root[key];
        ScopedCFTypeRef<CFStringRef> cf_key((CFStringRef)CFBridgingRetain(key));
        CFDictionarySetValue(mutable_root, cf_key, value);
      }
    }

    root_.reset(CFDictionaryCreateCopy(kCFAllocatorDefault, mutable_root));
  } else {
    write_root_.reset(CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
  }

  return true;
}

bool ConfigImplApple::CloseImpl() {
  if (path_.empty() || !dontread_) {
    root_.reset();
    return true;
  }

  NSUserDefaults* userDefaults =
      [[NSUserDefaults alloc] initWithSuiteName:@(kYassSuiteName)];
  [userDefaults setObject:CFBridgingRelease(CFRetain(write_root_)) forKey:@(kYassKeyName)];

  write_root_.reset();

  if (!path_.empty() && IsFile(path_) && ::unlink(path_.c_str()) == 0) {
    LOG(WARNING) << "removed legacy config file: " << path_;
  }

  path_.clear();

  return true;
}

bool ConfigImplApple::ReadImpl(const std::string& key, std::string* value) {
  CFStringRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(),
          (const void**)&obj) &&
      CFGetTypeID(obj) == CFStringGetTypeID()) {
    *value = SysCFStringRefToUTF8(obj);
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, bool* value) {
  CFBooleanRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(),
          (const void**)&obj) &&
      CFGetTypeID(obj) == CFBooleanGetTypeID()) {
    *value = CFBooleanGetValue(obj);
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, uint32_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(),
          (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() &&
      CFNumberGetValue(obj, kCFNumberSInt32Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, int32_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(),
          (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() &&
      CFNumberGetValue(obj, kCFNumberSInt32Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, uint64_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(),
          (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() &&
      CFNumberGetValue(obj, kCFNumberSInt64Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, int64_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(),
          (const void**)&obj) &&
      CFGetTypeID(obj) == CFNumberGetTypeID() &&
      CFNumberGetValue(obj, kCFNumberSInt64Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::WriteImpl(const std::string& key,
                                absl::string_view value) {
  ScopedCFTypeRef<CFStringRef> obj(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(value.data()),
      value.size(), kCFStringEncodingUTF8, FALSE));
  CFDictionarySetValue(
      write_root_,
      ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, bool value) {
  ScopedCFTypeRef<CFBooleanRef> obj(value ? kCFBooleanTrue : kCFBooleanFalse);
  CFDictionarySetValue(
      write_root_,
      ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, uint32_t value) {
  ScopedCFTypeRef<CFNumberRef> obj(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value));
  CFDictionarySetValue(
      write_root_,
      ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, int32_t value) {
  ScopedCFTypeRef<CFNumberRef> obj(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value));
  CFDictionarySetValue(
      write_root_,
      ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, uint64_t value) {
  ScopedCFTypeRef<CFNumberRef> obj(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value));
  CFDictionarySetValue(
      write_root_,
      ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, int64_t value) {
  ScopedCFTypeRef<CFNumberRef> obj(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value));
  CFDictionarySetValue(
      write_root_,
      ScopedCFTypeRef<CFStringRef>(SysUTF8ToCFStringRef(key)).get(), obj);
  return true;
}

}  // namespace config

#endif  // __APPLE__
