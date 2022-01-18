// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "config/config_impl_apple.hpp"

#ifdef __APPLE__

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <absl/flags/flag.h>
#include <memory>

#include "core/foundation_util.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"

ABSL_FLAG(std::string,
          configfile,
          "~/.yass/config.json",
          "load configs from file");

namespace {

std::string ExpandUser(const std::string& file_path) {
  std::string real_path = file_path;

  if (!real_path.empty() && real_path[0] == '~') {
    std::string home = getenv("HOME");
    return home + "/" + real_path.substr(2);
  }

  return real_path;
}

bool IsDirectory(const std::string& path) {
  struct stat Stat;
  if (::stat(path.c_str(), &Stat) != 0) {
    return false;
  }
  return S_ISDIR(Stat.st_mode);
}

bool CreatePrivateDirectory(const std::string& path) {
  return ::mkdir(path.c_str(), 0700) != 0;
}

bool EnsureCreatedDirectory(const std::string& path) {
  if (!IsDirectory(path) && !CreatePrivateDirectory(path)) {
    return false;
  }
  return true;
}

}  // anonymous namespace

namespace config {

bool ConfigImplApple::OpenImpl(bool dontread) {
  dontread_ = dontread;

  path_ = ExpandUser(absl::GetFlag(FLAGS_configfile));

  if (!dontread) {
    std::string content;
    NSError* error = nil;
    NSData* data = [NSData dataWithContentsOfFile:@(path_.c_str())
                                          options:NSDataReadingUncached
                                            error:&error];
    if (!data || error) {
      LOG(WARNING) << "configure file failed to read: " << path_
                   << " error: " << error;
      return false;
    }
    error = nil;
    root_ = (__bridge CFDictionaryRef)
        [NSJSONSerialization JSONObjectWithData:data
                                        options:NSJSONReadingMutableContainers
                                          error:&error];
    if (!root_ || error) {
      LOG(WARNING) << "bad configuration: " << error << " content: \"" << data
                   << "\"";
      return false;
    }

    VLOG(2) << "read from config file " << path_;
  } else {
    write_root_ = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
  }

  return true;
}

bool ConfigImplApple::CloseImpl() {
  if (path_.empty() || !dontread_) {
    return true;
  }

  auto dir_ref = Dirname(path_);
  std::string dir(dir_ref.data(), dir_ref.size());
  if (!EnsureCreatedDirectory(dir)) {
    LOG(WARNING) << "configure dir could not create: " << dir;
    return false;
  }

  NSError* error = nil;
  NSData* data = [NSJSONSerialization
      dataWithJSONObject:(__bridge NSMutableDictionary*)write_root_
                 options:NSJSONWritingPrettyPrinted
                   error:&error];
  if (!data || error) {
    LOG(WARNING) << "failed to write to path: \"" << path_ << " with error \""
                 << error << "\"";
    return false;
  }

  if (![data writeToFile:SysUTF8ToNSString(path_) atomically:FALSE]) {
    LOG(WARNING) << "failed to write to path: \"" << path_ << " with content \""
                 << data << "\"";
    return false;
  }

  VLOG(2) << "written from config file " << path_;

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
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, bool value) {
  CFBooleanRef obj = value ? kCFBooleanTrue : kCFBooleanFalse;
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, uint32_t value) {
  CFNumberRef obj =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, int32_t value) {
  CFNumberRef obj =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, uint64_t value) {
  CFNumberRef obj =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value);
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  return true;
}

bool ConfigImplApple::WriteImpl(const std::string& key, int64_t value) {
  CFNumberRef obj =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value);
  CFDictionarySetValue(write_root_,
                       (__bridge CFStringRef)SysUTF8ToNSString(key), obj);
  return true;
}

}  // namespace config

#endif  // __APPLE__
