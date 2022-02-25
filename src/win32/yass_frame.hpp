// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef YASS_WIN32_FRAME
#define YASS_WIN32_FRAME

#include "crypto/crypter_export.hpp"

#include <string>
#include <windows.h>

#include <CommCtrl.h>

class CYassFrame {
 public:
  CYassFrame();
  ~CYassFrame();

  BOOL Create(const wchar_t* className,
              const wchar_t* title,
              DWORD dwStyle,
              RECT rect,
              HINSTANCE hInstance);

 private:
  HWND m_hWnd;

 public:
  BOOL ShowWindow(int nCmdShow) { return ::ShowWindow(m_hWnd, nCmdShow); }

  BOOL SetForegroundWindow() { return ::SetForegroundWindow(m_hWnd); }

  BOOL UpdateWindow() { return ::UpdateWindow(m_hWnd); }

 public:
  std::string GetServerHost();
  std::string GetServerPort();
  std::string GetPassword();
  cipher_method GetMethod();
  std::string GetLocalHost();
  std::string GetLocalPort();
  std::string GetTimeout();
  std::string GetStatusMessage();

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
  HWND CreateStatic(const wchar_t* label,
                    const RECT& rect,
                    HWND pParentWnd,
                    UINT nID,
                    HINSTANCE hInstance) {
    return CreateWindowExW(
        0, WC_STATICW, label, WS_CHILD | WS_VISIBLE | SS_LEFT, rect.left,
        rect.top, rect.right - rect.left, rect.bottom - rect.top, pParentWnd,
        (HMENU)(UINT_PTR)nID, hInstance, nullptr);
  }

  HWND CreateEdit(DWORD dwStyle,
                  const RECT& rect,
                  HWND pParentWnd,
                  UINT nID,
                  HINSTANCE hInstance) {
    return CreateWindowExW(
        0, WC_EDITW, nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_BORDER | ES_LEFT | dwStyle,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        pParentWnd, (HMENU)(UINT_PTR)nID, hInstance, nullptr);
  }

  HWND CreateComboBox(DWORD dwStyle,
                      const RECT& rect,
                      HWND pParentWnd,
                      UINT nID,
                      HINSTANCE hInstance) {
    return CreateWindowExW(
        0, WC_COMBOBOXW, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | dwStyle,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        pParentWnd, (HMENU)(UINT_PTR)nID, hInstance, nullptr);
  }

  HWND CreateButton(const wchar_t* label,
                    DWORD dwStyle,
                    const RECT& rect,
                    HWND pParentWnd,
                    UINT nID,
                    HINSTANCE hInstance) {
    return CreateWindowExW(
        0, WC_BUTTONW, label, WS_CHILD | WS_VISIBLE | dwStyle, rect.left,
        rect.top, rect.right - rect.left, rect.bottom - rect.top, pParentWnd,
        (HMENU)(UINT_PTR)nID, hInstance, nullptr);
  }

  HWND server_host_label_;
  HWND server_port_label_;
  HWND password_label_;
  HWND method_label_;
  HWND local_host_label_;
  HWND local_port_label_;
  HWND timeout_label_;
  HWND autostart_label_;

  HWND server_host_edit_;
  HWND server_port_edit_;
  HWND password_edit_;
  HWND method_combo_box_;
  HWND local_host_edit_;
  HWND local_port_edit_;
  HWND timeout_edit_;
  HWND autostart_button_;

 protected:
  HWND CreateStatusBar(HWND pParentWnd,
                       int idStatus,
                       HINSTANCE hInstance,
                       int cParts) {
    HWND hwndStatus;
    RECT rcClient;
    HLOCAL hloc;
    PINT paParts;
    int i, nWidth;

    // Create the status bar.
    hwndStatus =
        CreateWindowEx(0,                      // no extended styles
                       STATUSCLASSNAME,        // name of status bar class
                       nullptr,                // no text when first created
                       WS_CHILD | WS_VISIBLE,  // creates a visible child window
                       0, 0, 0, 0,             // ignores size and position
                       pParentWnd,             // handle to parent window
                       (HMENU)(INT_PTR)idStatus,  // child window identifier
                       hInstance,  // handle to application instance
                       nullptr);   // no window creation data

    // Get the coordinates of the parent window's client area.
    ::GetClientRect(pParentWnd, &rcClient);

    // Allocate an array for holding the right edge coordinates.
    hloc = LocalAlloc(LHND, sizeof(int) * cParts);
    paParts = (PINT)LocalLock(hloc);

    // Calculate the right edge coordinate for each part, and
    // copy the coordinates to the array.
    nWidth = rcClient.right / cParts;
    int rightEdge = nWidth;
    for (i = 0; i < cParts; i++) {
      paParts[i] = rightEdge;
      rightEdge += nWidth;
    }

    // Tell the status bar to create the window parts.
    ::SendMessage(hwndStatus, SB_SETPARTS, (WPARAM)cParts, (LPARAM)paParts);

    // Free the array, and return.
    LocalUnlock(hloc);
    LocalFree(hloc);
    return hwndStatus;
  }
  HWND status_bar_;

 protected:
  void UpdateLayoutForDpi();
  void UpdateLayoutForDpi(UINT uDpi);

  // In the framework, when the user closes the frame window,
  // the window's default OnClose handler calls DestroyWindow.
  // When the main window closes, the application closes.
  void OnClose();

  // Implement Application shutdown
  // http://msdn.microsoft.com/en-us/library/ms700677(v=vs.85).aspx
  BOOL OnQueryEndSession();

  void OnUpdateStatusBar();

 protected:
  LRESULT OnDPIChanged(WPARAM w, LPARAM l);

 public:
  void OnStartButtonClicked();
  void OnStopButtonClicked();
  void OnCheckedAutoStartButtonClicked();

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
