// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CONFIG_CONFIG_IMPL_WINDOWS
#define H_CONFIG_CONFIG_IMPL_WINDOWS

#include "config/config_impl.hpp"

#ifdef _WIN32

#include <absl/strings/string_view.h>
#include <windows.h>
#include <string>
#include <vector>

#include "core/logging.hpp"

#define DEFAULT_CONFIG_KEY L"SOFTWARE\\YetAnotherShadowSocket"

namespace {

// documented in
// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/d7edc080-e499-4219-a837-1bc40b64bb04j
static_assert(sizeof(BYTE) == sizeof(uint8_t),
              "A BYTE is an 8-bit unsigned value that corresponds to a "
              "single octet in a network protocol.");

// documented in
// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/262627d8-3418-4627-9218-4ffe110850b2
static_assert(sizeof(DWORD) == sizeof(uint32_t),
              "A DWORD is a 32-bit unsigned integer.");

// depends on Windows SDK
#ifndef QWORD
typedef unsigned __int64 QWORD;
#endif  // QWORD

// documented in
// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/ac050bbf-a821-4fab-bccf-d95d892f428f
static_assert(sizeof(QWORD) == sizeof(uint64_t),
              "A QWORD is a 64-bit unsigned integer.");

// borrowed from sys_string_conversions_win.cc

// Converts between 8-bit and wide strings, using the given code page. The
// code page identifier is one accepted by the Windows function
// MultiByteToWideChar().
std::wstring SysMultiByteToWide(absl::string_view mb, uint32_t code_page);

std::string SysWideToMultiByte(const std::wstring& wide, uint32_t code_page);

// Do not assert in this function since it is used by the asssertion code!
std::string SysWideToUTF8(const std::wstring& wide) {
  return SysWideToMultiByte(wide, CP_UTF8);
}

// Do not assert in this function since it is used by the asssertion code!
std::wstring SysUTF8ToWide(absl::string_view utf8) {
  return SysMultiByteToWide(utf8, CP_UTF8);
}

std::string SysWideToNativeMB(const std::wstring& wide) {
  return SysWideToMultiByte(wide, CP_ACP);
}

std::wstring SysNativeMBToWide(absl::string_view native_mb) {
  return SysMultiByteToWide(native_mb, CP_ACP);
}

// Do not assert in this function since it is used by the asssertion code!
std::wstring SysMultiByteToWide(absl::string_view mb, uint32_t code_page) {
  int mb_length = static_cast<int>(mb.length());
  // Note that, if cbMultiByte is 0, the function fails.
  if (mb_length == 0)
    return std::wstring();

  // Compute the length of the buffer.
  int charcount = MultiByteToWideChar(
      code_page /* CodePage */, 0 /* dwFlags */, mb.data() /* lpMultiByteStr */,
      mb_length /* cbMultiByte */, nullptr /* lpWideCharStr */,
      0 /* cchWideChar */);
  // The function returns 0 if it does not succeed.
  if (charcount == 0)
    return std::wstring();

  // If the function succeeds and cchWideChar is 0,
  // the return value is the required size, in characters,
  std::wstring wide;
  wide.resize(charcount);
  MultiByteToWideChar(code_page /* CodePage */, 0 /* dwFlags */,
                      mb.data() /* lpMultiByteStr */,
                      mb_length /* cbMultiByte */, &wide[0] /* lpWideCharStr */,
                      charcount /* cchWideChar */);

  return wide;
}

// Do not assert in this function since it is used by the asssertion code!
std::string SysWideToMultiByte(const std::wstring& wide, uint32_t code_page) {
  int wide_length = static_cast<int>(wide.length());
  // If cchWideChar is set to 0, the function fails.
  if (wide_length == 0)
    return std::string();

  // Compute the length of the buffer we'll need.
  int charcount = WideCharToMultiByte(
      code_page /* CodePage */, 0 /* dwFlags */,
      wide.data() /* lpWideCharStr */, wide_length /* cchWideChar */,
      nullptr /* lpMultiByteStr */, 0 /* cbMultiByte */,
      nullptr /* lpDefaultChar */, nullptr /* lpUsedDefaultChar */);
  // The function returns 0 if it does not succeed.
  if (charcount == 0)
    return std::string();
  // If the function succeeds and cbMultiByte is 0, the return value is
  // the required size, in bytes, for the buffer indicated by lpMultiByteStr.
  std::string mb;
  mb.resize(charcount);

  WideCharToMultiByte(
      code_page /* CodePage */, 0 /* dwFlags */,
      wide.data() /* lpWideCharStr */, wide_length /* cchWideChar */,
      &mb[0] /* lpMultiByteStr */, charcount /* cbMultiByte */,
      nullptr /* lpDefaultChar */, nullptr /* lpUsedDefaultChar */);

  return mb;
}

std::wstring ExpandUserFromString(const wchar_t* path, size_t path_len) {
  // the return value is the REQUIRED number of TCHARs,
  // including the terminating NULL character.
  DWORD required_size = ::ExpandEnvironmentStringsW(path, nullptr, 0);

  /* if failure or too many bytes required, documented in
   * ExpandEnvironmentStringsW */
  if (required_size == 0 || required_size > 32 * 1024)
    return std::wstring(path, path_len ? path_len - 1 : path_len);

  std::wstring expanded_path;
  expanded_path.resize(required_size);
  ::ExpandEnvironmentStringsW(path, &expanded_path[0], required_size);

  return expanded_path;
}

bool ReadValue(HKEY hkey,
               const std::string& value,
               DWORD* type,
               std::vector<BYTE>* output) {
  DWORD BufferSize;
  std::wstring wvalue = SysUTF8ToWide(value);

  // If lpData is nullptr, and lpcbData is non-nullptr, the function returns
  // ERROR_SUCCESS and stores the size of the data, in bytes, in the variable
  // pointed to by lpcbData. This enables an application to determine the best
  // way to allocate a buffer for the value's data.
  if (::RegQueryValueExW(hkey /* HKEY */, wvalue.c_str() /* lpValueName */,
                         nullptr /* lpReserved */, type /* lpType */,
                         nullptr /* lpData */,
                         &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
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
  if (::RegQueryValueExW(hkey /* HKEY */, wvalue.c_str() /* lpValueName */,
                         nullptr /* lpReserved */, type /* lpType */,
                         output->data() /* lpData */,
                         &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
    return false;
  }
  output->resize(BufferSize);

  return true;
}

}  // anonymous namespace

namespace config {
class ConfigImplWindows : public ConfigImpl {
 public:
  ~ConfigImplWindows() override{};

 protected:
  bool OpenImpl(bool dontread) override {
    dontread_ = dontread;

    DWORD disposition;
    const wchar_t* subkey = DEFAULT_CONFIG_KEY;  // Allow to override it ?
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
    REGSAM samDesired =
        KEY_WOW64_64KEY | (dontread ? KEY_SET_VALUE : KEY_QUERY_VALUE);

    // Creates the specified registry key. If the key already exists, the
    // function opens it. Note that key names are not case sensitive.
    if (::RegCreateKeyExW(
            HKEY_CURRENT_USER /*hKey*/, subkey /* lpSubKey */, 0 /* Reserved */,
            nullptr /*lpClass*/, REG_OPTION_NON_VOLATILE /* dwOptions */,
            samDesired /* samDesired */, nullptr /*lpSecurityAttributes*/,
            &hkey_ /* phkResult */,
            &disposition /* lpdwDisposition*/) == ERROR_SUCCESS) {
      if (disposition == REG_CREATED_NEW_KEY) {
        VLOG(2) << "The key did not exist and was created: HKEY_CURRENT_USER/"
                << subkey;
      } else if (disposition == REG_OPENED_EXISTING_KEY) {
        VLOG(2) << "The key existed and was simply opened without being "
                   "changed: HKEY_CURRENT_USER/"
                << subkey;
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

  bool ReadImpl(const std::string& key, std::string* value) override {
    DWORD type;
    std::vector<BYTE> output;

    if (ReadValue(hkey_, key, &type, &output)) {
      wchar_t* raw_value = reinterpret_cast<wchar_t*>(output.data());
      size_t raw_len = output.size() / sizeof(wchar_t);
      if (raw_len)
        raw_value[raw_len - 1] = L'\0';

      if (type == REG_SZ) {
        *value = SysWideToUTF8(raw_value);
        return true;
      }

      if (type == REG_EXPAND_SZ) {
        std::wstring expanded_value = ExpandUserFromString(raw_value, raw_len);
        *value = SysWideToUTF8(expanded_value);
        return true;
      }
      /* fall through */
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
    std::vector<BYTE> output;

    if (ReadValue(hkey_, key, &type, &output) &&
        (type == REG_DWORD || type == REG_BINARY) &&
        output.size() == sizeof(DWORD)) {
      *value = *reinterpret_cast<uint32_t*>(output.data());
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
    std::vector<BYTE> output;

    if (ReadValue(hkey_, key, &type, &output) &&
        (type == REG_QWORD || type == REG_BINARY) &&
        output.size() == sizeof(*value)) {
      *value = *reinterpret_cast<uint64_t*>(output.data());
      return true;
    }

    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, int64_t* value) override {
    return ReadImpl(key, reinterpret_cast<uint64_t*>(value));
  }

  bool WriteImpl(const std::string& key, absl::string_view value) override {
    std::wstring wkey = SysUTF8ToWide(key);
    std::wstring wvalue = SysUTF8ToWide(value);

    if (::RegSetValueExW(
            hkey_ /* hKey*/, wkey.c_str() /*lpValueName*/, 0 /*Reserved*/,
            REG_SZ /*dwType*/,
            reinterpret_cast<const BYTE*>(wvalue.c_str()) /*lpData*/,
            (wvalue.size() + 1) * sizeof(wchar_t) /*cbData*/) ==
        ERROR_SUCCESS) {
      return true;
    }
    LOG(WARNING) << "failed to write field: " << key << " with content "
                 << value;
    return false;
  }

  bool WriteImpl(const std::string& key, bool value) override {
    uint32_t v = value ? 1 : 0;
    return WriteImpl(key, v);
  }

  bool WriteImpl(const std::string& key, uint32_t value) override {
    std::wstring wkey = SysUTF8ToWide(key);
    if (::RegSetValueExW(hkey_ /* hKey*/, wkey.c_str() /*lpValueName*/,
                         0 /*Reserved*/, REG_DWORD /*dwType*/,
                         reinterpret_cast<const BYTE*>(&value) /*lpData*/,
                         sizeof(value) /*cbData*/) == ERROR_SUCCESS) {
      return true;
    }
    LOG(WARNING) << "failed to write field: " << key << " with content "
                 << value;
    return false;
  }

  bool WriteImpl(const std::string& key, int32_t value) override {
    return WriteImpl(key, static_cast<uint32_t>(value));
  }

  bool WriteImpl(const std::string& key, uint64_t value) override {
    std::wstring wkey = SysUTF8ToWide(key);
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
  HKEY hkey_;
};
};  // namespace config

#endif  // _WIN32
#endif  // H_CONFIG_CONFIG_IMPL_WINDOWS
