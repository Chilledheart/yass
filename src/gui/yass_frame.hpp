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

  std::string GetServerHost();
  std::string GetServerPort();
  std::string GetPassword();
  std::string GetMethod();
  std::string GetLocalHost();
  std::string GetLocalPort();
  std::string GetTimeout();

  void Started();
  void Stopped();
  void StartFailed();

  void UpdateStatus();

private:
  void OnAbout(wxCommandEvent &event);
  void OnOption(wxCommandEvent &event);
#if wxCHECK_VERSION(3, 1, 3)
  void OnDPIChanged(wxDPIChangedEvent &event);
#endif

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
  ID_Option = 1,
};

#endif // YASS_FRAME
