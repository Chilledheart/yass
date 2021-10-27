// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef YASS_FRAME
#define YASS_FRAME

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

class LeftPanel;
class RightPanel;
class YASSFrame : public wxFrame {
public:
  YASSFrame(const wxString &title, const wxPoint &pos = wxDefaultPosition,
            const wxSize &size = wxDefaultSize);

  const char *GetServerHost();
  const char *GetServerPort();
  const char *GetPassword();
  const char *GetMethod();
  const char *GetLocalHost();
  const char *GetLocalPort();
  const char *GetTimeout();

  void Started();
  void Stopped();
  void StartFailed();

  void UpdateStatus();

private:
  void OnHello(wxCommandEvent &event);
  void OnAbout(wxCommandEvent &event);
  void OnOption(wxCommandEvent &event);
  void OnDIPChanged(wxDPIChangedEvent &event);

  void OnIdle(wxIdleEvent &event);
  void OnClose(wxCloseEvent &event);

  friend class YASSApp;
  LeftPanel *m_leftpanel = nullptr;
  RightPanel *m_rightpanel = nullptr;

private:
  wxDECLARE_EVENT_TABLE();

  uint64_t last_sync_time_ = 0;
  uint64_t last_rx_bytes_ = 0;
  uint64_t last_tx_bytes_ = 0;
  uint64_t rx_rate_ = 0;
  uint64_t tx_rate_ = 0;
};

enum {
  ID_Hello = 1,
  ID_Option = 2,
};

#endif // YASS_FRAME
