// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG_IMPL_WINDOWS
#define H_CONFIG_CONFIG_IMPL_WINDOWS

#include "config/config_impl.hpp"

#ifdef _WIN32

#include <windows.h>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "core/utils.hpp"

// TODO Allow to override it
static constexpr const wchar_t kYassSubkeyName[] = L"SOFTWARE\\YetAnotherShadowSocket";

namespace {

// documented in
// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/d7edc080-e499-4219-a837-1bc40b64bb04j
static_assert(sizeof(BYTE) == sizeof(uint8_t),
              "A BYTE is an 8-bit unsigned value that corresponds to a "
              "single octet in a network protocol.");

// documented in
// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/262627d8-3418-4627-9218-4ffe110850b2
static_assert(sizeof(DWORD) == sizeof(uint32_t), "A DWORD is a 32-bit unsigned integer.");

// depends on Windows SDK
#ifndef QWORD
typedef unsigned __int64 QWORD;
#endif  // QWORD

// documented in
// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/ac050bbf-a821-4fab-bccf-d95d892f428f
static_assert(sizeof(QWORD) == sizeof(uint64_t), "A QWORD is a 64-bit unsigned integer.");

bool HasValue(HKEY hkey, const std::string& value, DWORD* type, DWORD* size) {
  std::wstring wvalue = SysUTF8ToWide(value);

  if (::RegQueryValueExW(hkey /* HKEY */, wvalue.c_str() /* lpValueName */, nullptr /* lpReserved */, type /* lpType */,
                         nullptr /* lpData */, size /* lpcbData */) != ERROR_SUCCESS) {
    return false;
  }
  return true;
}

bool ReadValue(HKEY hkey, const std::string& value, DWORD* type, std::vector<BYTE>* output) {
  DWORD BufferSize;
  std::wstring wvalue = SysUTF8ToWide(value);
  output->clear();

  // If lpData is nullptr, and lpcbData is non-nullptr, the function returns
  // ERROR_SUCCESS and stores the size of the data, in bytes, in the variable
  // pointed to by lpcbData. This enables an application to determine the best
  // way to allocate a buffer for the value's data.
  if (::RegQueryValueExW(hkey /* HKEY */, wvalue.c_str() /* lpValueName */, nullptr /* lpReserved */, type /* lpType */,
                         nullptr /* lpData */, &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
    return false;
  }

  if (BufferSize > 32 * 1024) {
    return false;
  }

  // If the data has the REG_SZ, REG_MULTI_SZ or REG_EXPAND_SZ type,
  // the string may not have been stored with the proper terminating NULL
  // characters. Therefore, even if the function returns ERROR_SUCCESS, the
  // application should ensure that the string is properly terminated before
  // using it; otherwise, it may overwrite a buffer.
  output->resize(BufferSize);
  if (::RegQueryValueExW(hkey /* HKEY */, wvalue.c_str() /* lpValueName */, nullptr /* lpReserved */, type /* lpType */,
                         output->data() /* lpData */, &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
    return false;
  }
  output->resize(BufferSize);

  return true;
}

}  // anonymous namespace

namespace config {
class ConfigImplWindows : public ConfigImpl {
 public:
  ~ConfigImplWindows() override {}

 protected:
  bool OpenImpl(bool dontread) override {
    dontread_ = dontread;

    DWORD disposition;
    constexpr const wchar_t* subkey = kYassSubkeyName;
    // KEY_WOW64_64KEY: Indicates that an application on 32-bit Windows should:
    // Access a 64-bit key from either a 32-bit or 64-bit application.
    // Background: The registry in 64-bit versions of Windows is divided into
    // 32-bit and 64-bit keys. Many of the 32-bit keys have the same names
    // as their 64-bit counterparts, and vice versa.
    // With the opposite flag KEY_WOW64_32KEY, in the 64-bit version of program,
    // 32-bit keys are mapped under the HKEY_LOCAL_MACHINE\Software\WOW6432Node
    // registry key.
    //
    // https://docs.microsoft.com/en-us/troubleshoot/windows-client/deployment/view-system-registry-with-64-bit-windows

    // And no need the reg change notifications or operate on reg subkey
    REGSAM samDesired = KEY_WOW64_64KEY | (dontread ? KEY_SET_VALUE : KEY_QUERY_VALUE);

    // Creates the specified registry key. If the key already exists, the
    // function opens it. Note that key names are not case sensitive.
    if (::RegCreateKeyExW(HKEY_CURRENT_USER /*hKey*/, subkey /* lpSubKey */, 0 /* Reserved */, nullptr /*lpClass*/,
                          REG_OPTION_NON_VOLATILE /* dwOptions */, samDesired /* samDesired */,
                          nullptr /*lpSecurityAttributes*/, &hkey_ /* phkResult */,
                          &disposition /* lpdwDisposition*/) == ERROR_SUCCESS) {
      if (disposition == REG_CREATED_NEW_KEY) {
        std::cerr << "The key did not exist and was created: HKEY_CURRENT_USER/" << SysWideToUTF8(subkey) << std::endl;
      } else if (disposition == REG_OPENED_EXISTING_KEY) {
        std::cerr << "The key existed and was simply opened without being "
                     "changed: HKEY_CURRENT_USER/"
                  << SysWideToUTF8(subkey) << std::endl;
      }
      return true;
    }
    return false;
  }

  bool CloseImpl() override {
    bool ret = true;
    if (hkey_) {
      ret = ::RegCloseKey(hkey_) == ERROR_SUCCESS;
      hkey_ = nullptr;
    }
    return ret;
  }

  bool HasKeyStringImpl(const std::string& key) override {
    DWORD type, size;

    return HasValue(hkey_, key, &type, &size) && (type == REG_SZ || type == REG_EXPAND_SZ) &&
           size % sizeof(wchar_t) == 0;
  }

  bool HasKeyBoolImpl(const std::string& key) override { return HasKeyUint32Impl(key); }

  bool HasKeyUint32Impl(const std::string& key) override {
    DWORD type, size;

    return HasValue(hkey_, key, &type, &size) && (type == REG_DWORD || type == REG_BINARY) && size == sizeof(uint32_t);
  }

  bool HasKeyUint64Impl(const std::string& key) override {
    DWORD type, size;

    if (HasValue(hkey_, key, &type, &size) && (type == REG_QWORD || type == REG_BINARY) && size == sizeof(uint64_t)) {
      return true;
    }

    if (HasValue(hkey_, key, &type, &size) && (type == REG_DWORD || type == REG_BINARY) && size == sizeof(uint32_t)) {
      return true;
    }

    return false;
  }

  bool HasKeyInt32Impl(const std::string& key) override { return HasKeyUint32Impl(key); }

  bool HasKeyInt64Impl(const std::string& key) override { return HasKeyUint64Impl(key); }

  bool ReadImpl(const std::string& key, std::string* value) override {
    DWORD type;
    std::vector<BYTE> output;

    if (ReadValue(hkey_, key, &type, &output) && (type == REG_SZ || type == REG_EXPAND_SZ) &&
        output.size() % sizeof(wchar_t) == 0) {
      wchar_t* raw_value = reinterpret_cast<wchar_t*>(output.data());
      size_t raw_len = output.size() / sizeof(wchar_t);
      std::wstring raw_str;
      if (raw_len) {
        raw_value[raw_len - 1] = L'\0';
        raw_str = std::wstring(raw_value, raw_len - 1);
      }

      if (type == REG_SZ) {
        *value = SysWideToUTF8(raw_str);
        return true;
      }

      if (type == REG_EXPAND_SZ) {
        *value = SysWideToUTF8(ExpandUserFromString(raw_str));
        return true;
      }
      /* fall through */
    }

    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, bool* value) override {
    uint32_t v;
    bool ret = ReadImpl(key, &v);
    *value = v ? true : false;
    return ret;
  }

  bool ReadImpl(const std::string& key, uint32_t* value) override {
    DWORD type;
    std::vector<BYTE> output;

    if (ReadValue(hkey_, key, &type, &output) && (type == REG_DWORD || type == REG_BINARY) &&
        output.size() == sizeof(uint32_t)) {
      *value = *reinterpret_cast<uint32_t*>(output.data());
      return true;
    }

    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, int32_t* value) override {
    return ReadImpl(key, reinterpret_cast<uint32_t*>(value));
  }

  bool ReadImpl(const std::string& key, uint64_t* value) override {
    DWORD type;
    std::vector<BYTE> output;

    if (ReadValue(hkey_, key, &type, &output) && (type == REG_QWORD || type == REG_BINARY) &&
        output.size() == sizeof(uint64_t)) {
      *value = *reinterpret_cast<uint64_t*>(output.data());
      return true;
    }

    if (ReadValue(hkey_, key, &type, &output) && (type == REG_DWORD || type == REG_BINARY) &&
        output.size() == sizeof(uint32_t)) {
      *value = *reinterpret_cast<uint32_t*>(output.data());
      return true;
    }

    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, int64_t* value) override {
    return ReadImpl(key, reinterpret_cast<uint64_t*>(value));
  }

  bool WriteImpl(const std::string& key, std::string_view value) override {
    std::wstring wkey = SysUTF8ToWide(key);
    std::wstring wvalue = SysUTF8ToWide(value);

    if (::RegSetValueExW(hkey_ /* hKey*/, wkey.c_str() /*lpValueName*/, 0 /*Reserved*/, REG_SZ /*dwType*/,
                         reinterpret_cast<const BYTE*>(wvalue.c_str()) /*lpData*/,
                         (wvalue.size() + 1) * sizeof(wchar_t) /*cbData*/) == ERROR_SUCCESS) {
      return true;
    }
    std::cerr << "failed to write field: " << key << " with content " << value << std::endl;
    return false;
  }

  bool WriteImpl(const std::string& key, bool value) override {
    uint32_t v = value ? 1 : 0;
    return WriteImpl(key, v);
  }

  bool WriteImpl(const std::string& key, uint32_t value) override {
    std::wstring wkey = SysUTF8ToWide(key);
    if (::RegSetValueExW(hkey_ /* hKey*/, wkey.c_str() /*lpValueName*/, 0 /*Reserved*/, REG_DWORD /*dwType*/,
                         reinterpret_cast<const BYTE*>(&value) /*lpData*/, sizeof(value) /*cbData*/) == ERROR_SUCCESS) {
      return true;
    }
    std::cerr << "failed to write field: " << key << " with content " << value << std::endl;
    return false;
  }

  bool WriteImpl(const std::string& key, int32_t value) override {
    return WriteImpl(key, static_cast<uint32_t>(value));
  }

  bool WriteImpl(const std::string& key, uint64_t value) override {
    std::wstring wkey = SysUTF8ToWide(key);
    if (::RegSetValueExW(hkey_ /* hKey*/, wkey.c_str() /*lpValueName*/, 0 /*Reserved*/, REG_QWORD /*dwType*/,
                         reinterpret_cast<const BYTE*>(&value) /*lpData*/, sizeof(value) /*cbData*/) == ERROR_SUCCESS) {
      return true;
    }
    return false;
  }

  bool WriteImpl(const std::string& key, int64_t value) override {
    return WriteImpl(key, static_cast<uint64_t>(value));
  }

  bool DeleteImpl(const std::string& key) override {
    std::wstring wkey = SysUTF8ToWide(key);
    if (::RegDeleteValueW(hkey_ /*hKey*/, wkey.c_str() /*lpValueName*/) == ERROR_SUCCESS) {
      return true;
    }
    return false;
  }

 private:
  HKEY hkey_;
};
}  // namespace config

#endif  // _WIN32
#endif  // H_CONFIG_CONFIG_IMPL_WINDOWS
