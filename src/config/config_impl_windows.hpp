// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CONFIG_CONFIG_IMPL_WINDOWS
#define H_CONFIG_CONFIG_IMPL_WINDOWS

#include "config/config_impl.hpp"

#ifdef _WIN32

#include <codecvt>
#include <locale>
#include <memory>
#include <string>

using convert_type = std::codecvt_utf8<wchar_t>;

#include "core/logging.hpp"

#define DEFAULT_CONFIG_KEY L"SOFTWARE\\YetAnotherShadowSocket"

#include <windows.h>

namespace config {
class ConfigImplWindows : public ConfigImpl {
 public:
  ~ConfigImplWindows() override{};

 protected:
  bool OpenImpl(bool dontread) override {
    dontread_ = dontread;
    DWORD disp;
    REGSAM samDesired = KEY_WOW64_64KEY | (dontread ? KEY_WRITE : KEY_READ);

    // if not exist, create
    if (::RegCreateKeyExW(HKEY_CURRENT_USER /*hKey*/,
                          DEFAULT_CONFIG_KEY /* lpSubKey */, 0 /* Reserved */,
                          NULL /*lpClass*/,
                          REG_OPTION_NON_VOLATILE /* dwOptions */,
                          samDesired /* samDesired */,
                          NULL /*lpSecurityAttributes*/, &hkey_ /* phkResult */,
                          &disp /* lpdwDisposition*/) == ERROR_SUCCESS) {
      return true;
    }
    return false;
  }

  bool OpenImpl() override {
    ::RegCloseKey(hkey_);
    return true;
  }

  bool ReadImpl(const std::string& key, std::string* value) override {
    DWORD type;
    std::unique_ptr<char[]> output;
    size_t output_len;

    if (ReadReg(key, &type, output, &output_len) &&
        (type == REG_EXPAND_SZ || type == REG_SZ || type == REG_NONE ||
         type == REG_BINARY)) {
      wchar_t expanded_buf[MAX_PATH];
      const wchar_t* wvalue = reinterpret_cast<wchar_t*>(output.get());
      DWORD expanded_size = ::ExpandEnvironmentStringsW(
          wvalue, expanded_buf, sizeof(expanded_buf) / sizeof(expanded_buf[0]));
      std::wstring_convert<convert_type, wchar_t> converter;
      if (expanded_size == 0 || expanded_size > sizeof(expanded_buf)) {
        // the return value maybe contains terminating null characters.

        // use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
        *value = converter.to_bytes(wvalue);
      } else {
        // the return value is the number of TCHARs,
        // including the terminating null character.

        // use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
        *value = converter.to_bytes(expanded_buf);
      }
      return true;
    }

    LOG(WARNING) << "bad field: " << key;
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
    std::unique_ptr<char[]> output;
    size_t output_len;

    if (ReadReg(key, &type, output, &output_len) && type == REG_DWORD) {
      DCHECK_EQ(output_len, sizeof(uint32_t));
      memcpy(value, output.get(), sizeof(uint32_t));
      return true;
    }

    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, int32_t* value) override {
    return ReadImpl(key, reinterpret_cast<uint32_t*>(value));
  }

  bool ReadImpl(const std::string& key, uint64_t* value) override {
    DWORD type;
    std::unique_ptr<char[]> output;
    size_t output_len;

    if (ReadReg(key, &type, output, &output_len) && type == REG_QWORD) {
      DCHECK_EQ(output_len, sizeof(uint64_t));
      memcpy(value, output.get(), sizeof(uint64_t));
      return true;
    }

    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, int64_t* value) override {
    return ReadImpl(key, reinterpret_cast<uint64_t*>(value));
  }

  bool WriteImpl(const std::string& key, const std::string& value) override {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring wkey = converter.from_bytes(key);
    std::wstring wvalue = converter.from_bytes(value);
    if (::RegSetValueExW(
            hkey_ /* hKey*/, wkey.c_str() /*lpValueName*/, 0 /*Reserved*/,
            REG_SZ /*dwType*/,
            reinterpret_cast<const BYTE*>(wvalue.c_str()) /*lpData*/,
            (wvalue.size() + 1) * sizeof(wchar_t) /*cbData*/) ==
        ERROR_SUCCESS) {
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool WriteImpl(const std::string& key, bool value) override {
    uint32_t v = value ? 1 : 0;
    return WriteImpl(key, v);
  }

  bool WriteImpl(const std::string& key, uint32_t value) override {
    std::wstring wkey =
        std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(key);
    if (::RegSetValueExW(hkey_ /* hKey*/, wkey.c_str() /*lpValueName*/,
                         0 /*Reserved*/, REG_DWORD /*dwType*/,
                         reinterpret_cast<const BYTE*>(&value) /*lpData*/,
                         sizeof(value) /*cbData*/) == ERROR_SUCCESS) {
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool WriteImpl(const std::string& key, int32_t value) override {
    return WriteImpl(key, static_cast<uint32_t>(value));
  }

  bool WriteImpl(const std::string& key, uint64_t value) override {
    std::wstring wkey =
        std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(key);
    if (::RegSetValueExW(hkey_ /* hKey*/, wkey.c_str() /*lpValueName*/,
                         0 /*Reserved*/, REG_QWORD /*dwType*/,
                         reinterpret_cast<const BYTE*>(&value) /*lpData*/,
                         sizeof(value) /*cbData*/) == ERROR_SUCCESS) {
      return true;
    }
    return false;
  }

  bool WriteImpl(const std::string& key, int64_t value) override {
    return WriteImpl(key, static_cast<uint64_t>(value));
  }

 private:
  bool ReadReg(const std::string& key,
               DWORD* type,
               std::unique_ptr<char[]>& output,
               size_t* output_len) {
    DWORD BufferSize;
    std::wstring wkey =
        std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(key);
    if (::RegQueryValueExW(hkey_ /* HKEY */, wkey.c_str() /* lpValueName */,
                           NULL /* lpReserved */, type /* lpType */,
                           NULL /* lpData */,
                           &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
      return false;
    }
    if (BufferSize > 32 * 1024) {
      return false;
    }

    // If the data has the REG_SZ, REG_MULTI_SZ or REG_EXPAND_SZ type,
    // the string may not have been stored with the proper terminating null
    // characters. Therefore, even if the function returns ERROR_SUCCESS, the
    // application should ensure that the string is properly terminated before
    // using it; otherwise, it may overwrite a buffer.
    output = std::make_unique<char[]>(BufferSize);
    if (::RegQueryValueExW(hkey_ /* HKEY */, wkey.c_str() /* lpValueName */,
                           NULL /* lpReserved */, type /* lpType */,
                           reinterpret_cast<BYTE*>(output.get()) /* lpData */,
                           &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
      return false;
    }

    *output_len = BufferSize;
    return true;
  }

 private:
  HKEY hkey_;
};
};  // namespace config

#endif  // _WIN32
#endif  // H_CONFIG_CONFIG_IMPL_WINDOWS
