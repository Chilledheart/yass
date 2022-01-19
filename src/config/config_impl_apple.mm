// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "config/config_impl_apple.hpp"

#ifdef __APPLE__

#include <unistd.h>

#include <absl/flags/flag.h>
#include <memory>

#include "core/foundation_util.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"

ABSL_FLAG(std::string,
          configfile,
          "~/.yass/config.json",
          "load configs from file (legacy)");

namespace {

std::string ExpandUser(const std::string& file_path) {
  std::string real_path = file_path;

  if (!real_path.empty() && real_path[0] == '~') {
    std::string home = getenv("HOME");
    return home + "/" + real_path.substr(2);
  }

  return real_path;
}

void CFMutableContextOverwriteCallback(const void* key,
                                       const void* value,
                                       void* context) {
  CFMutableDictionaryRef mutable_root = (CFMutableDictionaryRef)context;
  CFDictionarySetValue(mutable_root, key, value);
}

void CFDirectionaryToUserDefaultCallback(const void* key,
                                         const void* value,
                                         void* context) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  [userDefaults setObject:(id)value forKey:(__bridge NSString*)key];
}

bool LoadConfigFromLegacyConfig(const std::string& path,
                                CFMutableDictionaryRef mutable_root) {
  NSError* error = nil;
  NSData* data = [NSData dataWithContentsOfFile:@(path.c_str())
                                        options:NSDataReadingUncached
                                          error:&error];
  CFDictionaryRef root;
  if (!data || error) {
    VLOG(1) << "legacy configure file failed to read: " << path
            << " error: " << error;
    return false;
  }
  error = nil;
  root = (__bridge CFDictionaryRef)
      [NSJSONSerialization JSONObjectWithData:data
                                      options:NSJSONReadingMutableContainers
                                        error:&error];
  if (!root || error) {
    VLOG(1) << "legacy configure file failed to parse: " << error
            << " content: \"" << data << "\"";
    return false;
  }

  CFDictionaryApplyFunction(root, CFMutableContextOverwriteCallback,
                            mutable_root);
  return true;
}

}  // anonymous namespace

namespace config {

bool ConfigImplApple::OpenImpl(bool dontread) {
  dontread_ = dontread;

  path_ = ExpandUser(absl::GetFlag(FLAGS_configfile));

  if (!dontread) {
    CFMutableDictionaryRef mutable_root = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    if (LoadConfigFromLegacyConfig(path_, mutable_root)) {
      VLOG(2) << "loaded from config file " << path_;
      ::unlink(path_.c_str());
    }

    // Overwrite all fields from UserDefaults
    CFDictionaryRef root = (__bridge CFDictionaryRef)[
        [NSUserDefaults standardUserDefaults] dictionaryRepresentation];
    CFDictionaryApplyFunction(root, CFMutableContextOverwriteCallback,
                              mutable_root);
    root_ = CFDictionaryCreateCopy(kCFAllocatorDefault, mutable_root);
    CFRelease(root);
    CFRelease(mutable_root);
  } else {
    write_root_ = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
  }

  return true;
}

bool ConfigImplApple::CloseImpl() {
  if (path_.empty() || !dontread_) {
    CFRelease(root_);
    return true;
  }

  CFDictionaryApplyFunction(write_root_, CFDirectionaryToUserDefaultCallback,
                            nil);

  CFRelease(write_root_);
  path_.clear();
  return true;
}

bool ConfigImplApple::ReadImpl(const std::string& key, std::string* value) {
  CFStringRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, (const void*)(__bridge CFStringRef)SysUTF8ToNSString(key),
          (const void**)&obj)) {
    *value = SysNSStringToUTF8((__bridge NSString*)obj);
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, bool* value) {
  CFBooleanRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, (const void*)(__bridge CFStringRef)SysUTF8ToNSString(key),
          (const void**)&obj)) {
    *value = CFBooleanGetValue(obj);
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, uint32_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, (const void*)(__bridge CFStringRef)SysUTF8ToNSString(key),
          (const void**)&obj) &&
      CFNumberGetValue(obj, kCFNumberSInt32Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, int32_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, (const void*)(__bridge CFStringRef)SysUTF8ToNSString(key),
          (const void**)&obj) &&
      CFNumberGetValue(obj, kCFNumberSInt32Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, uint64_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, (const void*)(__bridge CFStringRef)SysUTF8ToNSString(key),
          (const void**)&obj) &&
      CFNumberGetValue(obj, kCFNumberSInt64Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::ReadImpl(const std::string& key, int64_t* value) {
  CFNumberRef obj;
  if (CFDictionaryGetValueIfPresent(
          root_, (const void*)(__bridge CFStringRef)SysUTF8ToNSString(key),
          (const void**)&obj) &&
      CFNumberGetValue(obj, kCFNumberSInt64Type, value)) {
    return true;
  }
  LOG(WARNING) << "bad field: " << key;
  return false;
}

bool ConfigImplApple::WriteImpl(const std::string& key,
                                absl::string_view value) {
  CFStringRef obj = CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(value.data()),
      value.size(), kCFStringEncodingUTF8, FALSE);
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  CFRelease(obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, bool value) {
  CFBooleanRef obj = value ? kCFBooleanTrue : kCFBooleanFalse;
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  CFRelease(obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, uint32_t value) {
  CFNumberRef obj =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  CFRelease(obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, int32_t value) {
  CFNumberRef obj =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  CFRelease(obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, uint64_t value) {
  CFNumberRef obj =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value);
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  CFRelease(obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, int64_t value) {
  CFNumberRef obj =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value);
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  CFRelease(obj);
  return true;
}

}  // namespace config

#endif  // __APPLE__
