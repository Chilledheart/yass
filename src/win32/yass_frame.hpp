// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef YASS_WIN32_FRAME
#define YASS_WIN32_FRAME

#include "crypto/crypter_export.hpp"
#include <build/build_config.h>

#include <string>
#include <windows.h>

#ifdef COMPILER_MSVC
#include <CommCtrl.h>
#else
#include <commctrl.h>
#endif  // COMPILER_MSVC

class CYassFrame {
 public:
  CYassFrame();
  ~CYassFrame();

  BOOL Create(const wchar_t* className,
              const wchar_t* title,
              DWORD dwStyle,
              RECT rect,
              HINSTANCE hInstance,
              int nCmdShow);

  void CentreWindow();

  static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

 public:
  HWND Wnd() { return m_hWnd; }

 private:
  HWND m_hWnd;
  HINSTANCE m_hInstance;

 public:
  std::string GetServerHost();
  std::string GetServerSNI();
  std::string GetServerPort();
  std::string GetUsername();
  std::string GetPassword();
  cipher_method GetMethod();
  std::string GetLocalHost();
  std::string GetLocalPort();
  std::string GetTimeout();
  std::wstring GetStatusMessage();

  void OnStarted();
  void OnStopped();
  void OnStartFailed();

  void LoadConfig();

  // Left Panel
 protected:
  HWND start_button_;
  HWND stop_button_;

  // Right Panel
 protected:

  HWND server_host_label_;
  HWND server_sni_label_;
  HWND server_port_label_;
  HWND username_label_;
  HWND password_label_;
  HWND method_label_;
  HWND local_host_label_;
  HWND local_port_label_;
  HWND timeout_label_;
  HWND autostart_label_;
  HWND systemproxy_label_;

  HWND server_host_edit_;
  HWND server_sni_edit_;
  HWND server_port_edit_;
  HWND username_edit_;
  HWND password_edit_;
  HWND method_combo_box_;
  HWND local_host_edit_;
  HWND local_port_edit_;
  HWND timeout_edit_;
  HWND autostart_button_;
  HWND systemproxy_button_;

 protected:
  HWND status_bar_;

 protected:
  void UpdateLayoutForDpi();
  void UpdateLayoutForDpi(UINT uDpi);

  void OnClose();
  BOOL OnQueryEndSession();

  void OnUpdateStatusBar();
  std::wstring previous_status_message_;

 protected:
  LRESULT OnDPIChanged(WPARAM w, LPARAM l);

 public:
  void OnStartButtonClicked();
  void OnStopButtonClicked();
  void OnCheckedAutoStartButtonClicked();
  void OnCheckedSystemProxyButtonClicked();

 public:
  void OnAppOption();
  static INT_PTR CALLBACK OnAppOptionMessage(HWND hDlg,
                                             UINT message,
                                             WPARAM wParam,
                                             LPARAM lParam);
  void OnAppAbout();
  static INT_PTR CALLBACK OnAppAboutMessage(HWND hDlg,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam);

 private:
  friend class CYassApp;

 private:
  uint64_t last_sync_time_ = 0;
  uint64_t last_rx_bytes_ = 0;
  uint64_t last_tx_bytes_ = 0;
  uint64_t rx_rate_ = 0;
  uint64_t tx_rate_ = 0;
};

#endif  // YASS_WIN32_FRAME
