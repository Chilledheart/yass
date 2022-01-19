// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "gui/yass_frame.hpp"

#include <absl/flags/flag.h>
#include <wx/stattext.h>
#include <iomanip>
#include <sstream>

#include "cli/socks5_connection_stats.hpp"
#include "core/utils.hpp"
#include "gui/option_dialog.hpp"
#include "gui/panels.hpp"
#include "gui/yass.hpp"

static void humanReadableByteCountBin(std::ostream* ss, uint64_t bytes) {
  if (bytes < 1024) {
    *ss << bytes << " B";
    return;
  }
  uint64_t value = bytes;
  char ci[] = {"KMGTPE"};
  const char* c = ci;
  for (int i = 40; i >= 0 && bytes > 0xfffccccccccccccLU >> i; i -= 10) {
    value >>= 10;
    ++c;
  }
  *ss << std::fixed << std::setw(5) << std::setprecision(2) << value / 1024.0
      << " " << *c;
}

YASSFrame::YASSFrame(const wxString& title,
                     const wxPoint& pos,
                     const wxSize& size)
    : wxFrame(nullptr,
              wxID_ANY,
              title,
              pos,
              size,
              wxDEFAULT_FRAME_STYLE & (~wxMAXIMIZE_BOX) & (~wxRESIZE_BORDER)) {
  wxMenu* menuFile = new wxMenu;
  menuFile->Append(ID_Option, wxT("&Option...\tCtrl-O"),
                   wxT("More Options for this applications"));

  wxMenu* menuHelp = new wxMenu;
  menuHelp->Append(wxID_ABOUT);

  wxMenuBar* menuBar = new wxMenuBar;
  menuBar->Append(menuFile, wxT("&File"));
  menuBar->Append(menuHelp, wxT("&Help"));

  SetMenuBar(menuBar);

  CreateStatusBar();
  SetStatusText(wxT("READY"));

  wxPanel* panel = new wxPanel(this, wxID_ANY);

  wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);

  m_leftpanel = new LeftPanel(panel);
  m_rightpanel = new RightPanel(panel);

  hbox->Add(m_leftpanel, 1, wxEXPAND | wxALL, 5);
  hbox->Add(m_rightpanel, 1, wxEXPAND | wxALL, 5);

  panel->SetSizer(hbox);
}

std::string YASSFrame::GetServerHost() {
  return m_rightpanel->m_serverhost_tc->GetValue().operator const char*();
}

std::string YASSFrame::GetServerPort() {
  return m_rightpanel->m_serverport_tc->GetValue().operator const char*();
}

std::string YASSFrame::GetPassword() {
  return m_rightpanel->m_password_tc->GetValue().operator const char*();
}

std::string YASSFrame::GetMethod() {
  return m_rightpanel->m_method_tc->GetStringSelection().operator const char*();
}

std::string YASSFrame::GetLocalHost() {
  return m_rightpanel->m_localhost_tc->GetValue().operator const char*();
}

std::string YASSFrame::GetLocalPort() {
  return m_rightpanel->m_localport_tc->GetValue().operator const char*();
}

std::string YASSFrame::GetTimeout() {
  return m_rightpanel->m_timeout_tc->GetValue().operator const char*();
}

void YASSFrame::Started() {
  UpdateStatus();
  m_rightpanel->m_serverhost_tc->SetEditable(false);
  m_rightpanel->m_serverport_tc->SetEditable(false);
  m_rightpanel->m_password_tc->SetEditable(false);
  m_rightpanel->m_method_tc->Enable(false);
  m_rightpanel->m_localhost_tc->SetEditable(false);
  m_rightpanel->m_localport_tc->SetEditable(false);
  m_rightpanel->m_timeout_tc->SetEditable(false);
  m_leftpanel->m_stop->Enable();
}

void YASSFrame::StartFailed() {
  UpdateStatus();
  m_rightpanel->m_serverhost_tc->SetEditable(true);
  m_rightpanel->m_serverport_tc->SetEditable(true);
  m_rightpanel->m_password_tc->SetEditable(true);
  m_rightpanel->m_method_tc->Enable(true);
  m_rightpanel->m_localhost_tc->SetEditable(true);
  m_rightpanel->m_localport_tc->SetEditable(true);
  m_rightpanel->m_timeout_tc->SetEditable(true);
  m_leftpanel->m_start->Enable();
  wxMessageBox(mApp->GetStatus(), wxT("About YASS"),
               wxOK | wxICON_ERROR);
}

void YASSFrame::Stopped() {
  UpdateStatus();
  m_rightpanel->m_serverhost_tc->SetEditable(true);
  m_rightpanel->m_serverport_tc->SetEditable(true);
  m_rightpanel->m_password_tc->SetEditable(true);
  m_rightpanel->m_method_tc->Enable(true);
  m_rightpanel->m_localhost_tc->SetEditable(true);
  m_rightpanel->m_localport_tc->SetEditable(true);
  m_rightpanel->m_timeout_tc->SetEditable(true);
  m_leftpanel->m_start->Enable();
}

void YASSFrame::UpdateStatus() {
  m_rightpanel->m_serverhost_tc->SetValue(absl::GetFlag(FLAGS_server_host));
  m_rightpanel->m_serverport_tc->SetValue(
      std::to_string(absl::GetFlag(FLAGS_server_port)));
  m_rightpanel->m_password_tc->SetValue(absl::GetFlag(FLAGS_password));
  m_rightpanel->m_method_tc->SetStringSelection(to_cipher_method_str(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method))));
  m_rightpanel->m_localhost_tc->SetValue(absl::GetFlag(FLAGS_local_host));
  m_rightpanel->m_localport_tc->SetValue(
      std::to_string(absl::GetFlag(FLAGS_local_port)));
  m_rightpanel->m_timeout_tc->SetValue(
      std::to_string(absl::GetFlag(FLAGS_connect_timeout)));

  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND / 10) {
    uint64_t rx_bytes = total_rx_bytes;
    uint64_t tx_bytes = total_tx_bytes;
    rx_rate_ = static_cast<double>(rx_bytes - last_rx_bytes_) / delta_time *
               NS_PER_SECOND;
    tx_rate_ = static_cast<double>(tx_bytes - last_tx_bytes_) / delta_time *
               NS_PER_SECOND;
    last_sync_time_ = sync_time;
    last_rx_bytes_ = rx_bytes;
    last_tx_bytes_ = tx_bytes;
  }

  std::stringstream ss;
  ss << mApp->GetStatus();
  ss << " tx rate: ";
  humanReadableByteCountBin(&ss, rx_rate_);
  ss << "/s";
  ss << " rx rate: ";
  humanReadableByteCountBin(&ss, tx_rate_);
  ss << "/s";

  SetStatusText(ss.str());
}

void YASSFrame::OnOption(wxCommandEvent& WXUNUSED(event)) {
#if wxCHECK_VERSION(3, 1, 0)
  wxSize size(this->FromDIP(wxSize(400, 240)));
#else
  wxSize size(400, 240);
#endif
  OptionDialog dialog(this, wxT("YASS Option"), wxDefaultPosition, size);
  if (dialog.ShowModal() == wxID_OK) {
    mApp->SaveConfigToDisk();
  }
}

void YASSFrame::OnAbout(wxCommandEvent& WXUNUSED(event)) {
  wxMessageBox(wxT("This is Yet Another Shadow Socket"), wxT("About YASS"),
               wxOK | wxICON_INFORMATION);
}

#if wxCHECK_VERSION(3, 1, 3)
/* Currently this event is generated by wxMSW port. */
void YASSFrame::OnDPIChanged(wxDPIChangedEvent& event) {
  wxSize newSize = GetSize();
  newSize.x *= static_cast<double>(event.GetNewDPI().GetWidth()) /
               event.GetOldDPI().GetWidth();
  newSize.y *= static_cast<double>(event.GetNewDPI().GetHeight()) /
               event.GetOldDPI().GetHeight();
  SetSize(newSize);

  newSize = m_rightpanel->GetSize();
  newSize.x *= static_cast<double>(event.GetNewDPI().GetWidth()) /
               event.GetOldDPI().GetWidth();
  newSize.y *= static_cast<double>(event.GetNewDPI().GetHeight()) /
               event.GetOldDPI().GetHeight();
  m_rightpanel->SetSize(newSize);
}
#endif  // wxCHECK_VERSION

void YASSFrame::OnIdle(wxIdleEvent& WXUNUSED(event)) {
  if (mApp->GetState() == YASSApp::STARTED) {
    UpdateStatus();
  }
}

void YASSFrame::OnClose(wxCloseEvent& /*event*/) {
  LOG(WARNING) << "Frame is closing ";
  mApp->Exit();
}

wxBEGIN_EVENT_TABLE(YASSFrame, wxFrame)
  EVT_MENU(ID_Option, YASSFrame::OnOption)
  EVT_MENU(wxID_ABOUT, YASSFrame::OnAbout)
#if wxCHECK_VERSION(3, 1, 3)
  EVT_DPI_CHANGED(YASSFrame::OnDPIChanged)
#endif  // wxCHECK_VERSION
  EVT_IDLE(YASSFrame::OnIdle)
  EVT_CLOSE(YASSFrame::OnClose)
wxEND_EVENT_TABLE()
