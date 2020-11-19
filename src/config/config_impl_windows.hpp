//
// config_impl_windows.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CONFIG_CONFIG_IMPL_WINDOWS
#define H_CONFIG_CONFIG_IMPL_WINDOWS

#include "config/config_impl.hpp"

#ifdef _WIN32

#include <memory>
#include <string>

#include "core/logging.hpp"

#define DEFAULT_CONFIG_KEY  "SOFTWARE\\YetAnotherShadowSocket"

#include <windows.h>

namespace config {
class ConfigImplWindows : public ConfigImpl {
public:
  ~ConfigImplWindows() override{};

  bool Open(bool dontread) override {
    dontread_ = dontread;
    DWORD disp;
    REGSAM samDesired =
      KEY_WOW64_64KEY |
      (dontread ? KEY_WRITE : KEY_READ);

    // if not exist, create
    if (::RegCreateKeyExA(HKEY_CURRENT_USER /*hKey*/,
            DEFAULT_CONFIG_KEY /* lpSubKey */,
            0 /* Reserved */,
            NULL /*lpClass*/,
            REG_OPTION_NON_VOLATILE /* dwOptions */,
            samDesired /* samDesired */,
            NULL /*lpSecurityAttributes*/,
            &hkey_ /* phkResult */,
            &disp /* lpdwDisposition*/) == ERROR_SUCCESS) {
      return true;
    }
    return false;
  }

  bool Close() override {
    ::RegCloseKey(hkey_);
    return true;
  }

  bool Read(const std::string &key, std::string *value) override {
    DWORD type;
    std::unique_ptr<char[]> output;
    size_t output_len;

    if (ReadReg(key, &type, output, &output_len) &&
        (type == REG_EXPAND_SZ || type == REG_SZ || type == REG_NONE ||
         type == REG_BINARY)) {
      char expanded_buf[MAX_PATH];
      DWORD expanded_size = ::ExpandEnvironmentStringsA(
          output.get(), expanded_buf, sizeof(expanded_buf));
      if (expanded_size == 0 || expanded_size > sizeof(expanded_buf)) {
        // the return value maybe contains terminating null characters.
        while (output_len && *(output.get() + output_len - 1) == '\0') {
          output_len--;
        }
        *value = std::string(output.get(), output_len);
      } else {
        // the return value is the number of TCHARs,
        // including the terminating null character.
        *value = std::string(expanded_buf, expanded_size - 1);
      }
      return true;
    }

    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool Read(const std::string &key, uint32_t *value) override {
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

  bool Read(const std::string &key, int32_t *value) override {
    return Read(key, reinterpret_cast<uint32_t *>(value));
  }

  bool Read(const std::string &key, uint64_t *value) override {
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

  bool Read(const std::string &key, int64_t *value) override {
    return Read(key, reinterpret_cast<uint64_t *>(value));
  }

  bool Write(const std::string &key, const std::string &value) override {
    if (::RegSetValueExA(hkey_ /* hKey*/,
                         key.c_str() /*lpValueName*/,
                         0 /*Reserved*/,
                         REG_SZ /*dwType*/,
                         reinterpret_cast<const BYTE*>(value.c_str()) /*lpData*/,
                         value.size() + 1 /*cbData*/) == ERROR_SUCCESS) {
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool Write(const std::string &key, uint32_t value) override {
    if (::RegSetValueExA(hkey_ /* hKey*/,
                         key.c_str() /*lpValueName*/,
                         0 /*Reserved*/,
                         REG_DWORD /*dwType*/,
                         reinterpret_cast<const BYTE*>(&value) /*lpData*/,
                         sizeof(value) /*cbData*/) == ERROR_SUCCESS) {
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool Write(const std::string &key, int32_t value) override {
    return Write(key, static_cast<uint32_t>(value));
  }

  bool Write(const std::string &key, uint64_t value) override {
    if (::RegSetValueExA(hkey_ /* hKey*/,
                         key.c_str() /*lpValueName*/,
                         0 /*Reserved*/,
                         REG_QWORD /*dwType*/,
                         reinterpret_cast<const BYTE*>(&value) /*lpData*/,
                         sizeof(value) /*cbData*/) == ERROR_SUCCESS) {
      return true;
    }
    return false;
  }

  bool Write(const std::string &key, int64_t value) override {
    return Write(key, static_cast<uint64_t>(value));
  }

private:
  bool ReadReg(const std::string &key, DWORD *type,
      std::unique_ptr<char[]> &output, size_t *output_len) {
    DWORD BufferSize;
    if (::RegQueryValueExA(hkey_ /* HKEY */,
          key.c_str() /* lpValueName */,
          NULL /* lpReserved */,
          type /* lpType */,
          NULL /* lpData */,
          &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
      return false;
    }
    if (BufferSize > 32 * 1024) {
      return false;
    }

    // If the data has the REG_SZ, REG_MULTI_SZ or REG_EXPAND_SZ type,
    // the string may not have been stored with the proper terminating null characters.
    // Therefore, even if the function returns ERROR_SUCCESS,
    // the application should ensure that the string is properly terminated
    // before using it; otherwise, it may overwrite a buffer.
    output = std::make_unique<char[]>(BufferSize);
    if (::RegQueryValueExA(hkey_ /* HKEY */,
          key.c_str() /* lpValueName */,
          NULL /* lpReserved */,
          type /* lpType */,
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
}; // namespace config

#endif // _WIN32
#endif // H_CONFIG_CONFIG_IMPL_WINDOWS
