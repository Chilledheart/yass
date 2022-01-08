// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */

#include "gui/utils.hpp"

#include "core/logging.hpp"
#ifdef _WIN32

#define DEFAULT_AUTOSTART_KEY \
  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"

#include <Tchar.h>
#include <shellscalingapi.h>
#include <windows.h>

HANDLE EnsureShcoreLoaded() {
  return LoadLibraryExW(L"Shcore.dll", 0, 0);
}

typedef HRESULT(__stdcall* PFNSETPROCESSDPIAWARENESS)(PROCESS_DPI_AWARENESS);

static LONG get_win_run_key(HKEY* pKey) {
  const wchar_t* key_run = DEFAULT_AUTOSTART_KEY;
  LONG result =
      RegOpenKeyExW(HKEY_CURRENT_USER, key_run, 0L, KEY_WRITE | KEY_READ, pKey);

  return result;
}

static int add_to_auto_start(const wchar_t* appname_w, const wchar_t* path_w) {
  HKEY hKey;
  LONG result = get_win_run_key(&hKey);
  if (result != ERROR_SUCCESS) {
    return -1;
  }

  DWORD n = sizeof(wchar_t) * (wcslen(path_w) + 1);

  result = RegSetValueExW(hKey, appname_w, 0, REG_SZ, (const BYTE*)path_w, n);

  RegCloseKey(hKey);
  if (result != ERROR_SUCCESS) {
    return -1;
  }

  return 0;
}

static int delete_from_auto_start(const wchar_t* appname) {
  HKEY hKey;
  LONG result = get_win_run_key(&hKey);
  if (result != ERROR_SUCCESS) {
    return -1;
  }

  result = RegDeleteValueW(hKey, appname);
  RegCloseKey(hKey);
  if (result != ERROR_SUCCESS) {
    return -1;
  }

  return 0;
}

static int get_yass_auto_start() {
  HKEY hKey;
  LONG result = get_win_run_key(&hKey);
  if (result != ERROR_SUCCESS) {
    return -1;
  }

  char buf[MAX_PATH] = {0};
  DWORD len = sizeof(buf);
  result = RegQueryValueExW(hKey,                       /* Key */
                            _T(DEFAULT_AUTOSTART_NAME), /* value */
                            nullptr,                    /* reserved */
                            nullptr,                    /* output type */
                            (LPBYTE)buf,                /* output data */
                            &len);                      /* output length */

  RegCloseKey(hKey);
  if (result != ERROR_SUCCESS) {
    /* yass applet auto start no set  */
    return 0;
  }

  return 1;
}

static int set_yass_auto_start(bool on) {
  int result = 0;
  if (on) {
    /* turn on auto start  */
    wchar_t applet_path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, applet_path, MAX_PATH) == 0) {
      return -1;
    }

    result = add_to_auto_start(_T(DEFAULT_AUTOSTART_NAME), applet_path);

  } else {
    /* turn off auto start */
    result = delete_from_auto_start(_T(DEFAULT_AUTOSTART_NAME));
  }
  return result;
}

bool Utils::GetAutoStart() {
  int ret = get_yass_auto_start();
  return ret == 1;
}

void Utils::EnableAutoStart(bool on) {
  set_yass_auto_start(on);
}

bool Utils::SetProcessDpiAwareness() {
  HANDLE hLibrary = EnsureShcoreLoaded();
  PFNSETPROCESSDPIAWARENESS const SetProcessDpiAwareness =
      (PFNSETPROCESSDPIAWARENESS)GetProcAddress((HMODULE)hLibrary,
                                                "SetProcessDpiAwareness");
  if (SetProcessDpiAwareness == nullptr) {
    return false;
  }

  HRESULT hr = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  if (FAILED(hr)) {
    return false;
  }
  return true;
}

uint64_t Utils::GetMonotonicTime() {
  static LARGE_INTEGER StartTime, Frequency;
  static bool started;

  LARGE_INTEGER CurrentTime, ElapsedNanoseconds;

  if (!started) {
    if (!QueryPerformanceFrequency(&Frequency)) {
      LOG(WARNING) << "QueryPerformanceFrequency failed";
      return 0;
    }
    if (!QueryPerformanceCounter(&StartTime)) {
      LOG(WARNING) << "QueryPerformanceCounter failed";
      return 0;
    }
    started = true;
  }
  // Activity to be timed
  if (!QueryPerformanceCounter(&CurrentTime)) {
    LOG(WARNING) << "QueryPerformanceCounter failed";
    return 0;
  }

  ElapsedNanoseconds.QuadPart = CurrentTime.QuadPart - StartTime.QuadPart;

  //
  // We now have the elapsed number of ticks, along with the
  // number of ticks-per-second. We use these values
  // to convert to the number of elapsed microseconds.
  // To guard against loss-of-precision, we convert
  // to microseconds *before* dividing by ticks-per-second.
  //

  ElapsedNanoseconds.QuadPart =
      (double)ElapsedNanoseconds.QuadPart * NS_PER_SECOND / Frequency.QuadPart;

  return ElapsedNanoseconds.QuadPart;
}

#endif
