// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "gui/yass_frame.hpp"

#include "cli/socks5_connection_stats.hpp"
#include "gui/panels.hpp"
#include "gui/utils.hpp"
#include "gui/yass.hpp"
#include <iomanip>
#include <sstream>
#include <wx/stattext.h>

static void humanReadableByteCountBin(std::ostream *ss, uint64_t bytes) {
  if (bytes < 1024) {
    *ss << bytes << " B";
    return;
  }
  uint64_t value = bytes;
  char ci[] = {"KMGTPE"};
  const char *c = ci;
  for (unsigned i = 40; i >= 0 && bytes > 0xfffccccccccccccLU >> i; i -= 10) {
    value >>= 10;
    ++c;
  }
  *ss << std::fixed << std::setw(5) << std::setprecision(2) << value / 1024.0
      << " " << *c;
}

YASSFrame::YASSFrame(const wxString &title, const wxPoint &pos,
                     const wxSize &size)
    : wxFrame(NULL, wxID_ANY, title, pos, size,
              wxDEFAULT_FRAME_STYLE & (~wxMAXIMIZE_BOX) & (~wxRESIZE_BORDER)) {
  wxMenu *menuFile = new wxMenu;
  menuFile->Append(ID_Hello, wxT("&Hello...\tCtrl-H"),
                   wxT("Hell string shown in status bar for this menu item"));

  wxMenu *menuHelp = new wxMenu;
  menuHelp->Append(wxID_ABOUT);

  wxMenuBar *menuBar = new wxMenuBar;
  menuBar->Append(menuFile, wxT("&File"));
  menuBar->Append(menuHelp, wxT("&Help"));

  SetMenuBar(menuBar);

  CreateStatusBar();
  SetStatusText(wxT("READY"));

  wxPanel *panel = new wxPanel(this, wxID_ANY);

  wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);

  m_leftpanel = new LeftPanel(panel);
  m_rightpanel = new RightPanel(panel);

  hbox->Add(m_leftpanel, 1, wxEXPAND | wxALL, 5);
  hbox->Add(m_rightpanel, 1, wxEXPAND | wxALL, 5);

  panel->SetSizer(hbox);

#ifdef _WIN32
  SetIcon(wxICON(IDI_ICON1));
#endif

  Centre();
}

std::string YASSFrame::GetServerHost() {
  return std::string(m_rightpanel->m_serverhost_tc->GetValue());
}

std::string YASSFrame::GetServerPort() {
  return std::string(m_rightpanel->m_serverport_tc->GetValue());
}

std::string YASSFrame::GetPassword() {
  return std::string(m_rightpanel->m_password_tc->GetValue());
}

std::string YASSFrame::GetMethod() {
  return std::string(m_rightpanel->m_method_tc->GetStringSelection());
}

std::string YASSFrame::GetLocalHost() {
  return std::string(m_rightpanel->m_localhost_tc->GetValue());
}

std::string YASSFrame::GetLocalPort() {
  return std::string(m_rightpanel->m_localport_tc->GetValue());
}

std::string YASSFrame::GetTimeout() {
  return std::string(m_rightpanel->m_timeout_tc->GetValue());
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
}

void YASSFrame::Stopped() {
  UpdateStatus();
  m_rightpanel->m_serverhost_tc->SetEditable(true);
  m_rightpanel->m_serverport_tc->SetEditable(true);
  m_rightpanel->m_password_tc->SetEditable(true);
  m_rightpanel->m_method_tc->Enable(true);
  m_rightpanel->m_localhost_tc->SetEditable(true);
  m_rightpanel->m_localport_tc->SetEditable(true);
  m_leftpanel->m_start->Enable();
}

void YASSFrame::UpdateStatus() {
  m_rightpanel->m_serverhost_tc->SetValue(FLAGS_server_host);
  m_rightpanel->m_serverport_tc->SetValue(std::to_string(FLAGS_server_port));
  m_rightpanel->m_password_tc->SetValue(FLAGS_password);
  m_rightpanel->m_method_tc->SetStringSelection(FLAGS_method);
  m_rightpanel->m_localhost_tc->SetValue(FLAGS_local_host);
  m_rightpanel->m_localport_tc->SetValue(std::to_string(FLAGS_local_port));
  m_rightpanel->m_timeout_tc->SetValue(std::to_string(FLAGS_timeout));

  uint64_t sync_time = Utils::GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND / 10) {
    uint64_t rx_bytes = total_rx_bytes;
    uint64_t tx_bytes = total_tx_bytes;
    rx_rate_ = (double)(rx_bytes - last_rx_bytes_) / delta_time * NS_PER_SECOND;
    tx_rate_ = (double)(tx_bytes - last_tx_bytes_) / delta_time * NS_PER_SECOND;
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

void YASSFrame::OnHello(wxCommandEvent &WXUNUSED(event)) {
  wxLogMessage(wxT("Hello from YASS!"));
}

void YASSFrame::OnAbout(wxCommandEvent &WXUNUSED(event)) {
  wxMessageBox(wxT("This is Yet-Another-Shadow-Socket"), wxT("About YASS"),
               wxOK | wxICON_INFORMATION);
}

void YASSFrame::OnDIPChanged(wxDPIChangedEvent &event) {
  wxLogMessage(wxT("old dpi %u/%u new dpi %u/%u"),
    event.GetOldDPI().GetWidth(), event.GetOldDPI().GetHeight(),
    event.GetNewDPI().GetWidth(), event.GetNewDPI().GetHeight());
  wxSize newSize = GetSize();
  newSize.x *= (double)event.GetNewDPI().GetWidth() / event.GetOldDPI().GetWidth();
  newSize.y *= (double)event.GetNewDPI().GetHeight() / event.GetOldDPI().GetHeight();
  SetSize(newSize);

  newSize = m_rightpanel->GetSize();
  newSize.x *= (double)event.GetNewDPI().GetWidth() / event.GetOldDPI().GetWidth();
  newSize.y *= (double)event.GetNewDPI().GetHeight() / event.GetOldDPI().GetHeight();
  m_rightpanel->SetSize(newSize);
}

void YASSFrame::OnIdle(wxIdleEvent &WXUNUSED(event)) {
  if (mApp->GetState() == YASSApp::STARTED) {
    UpdateStatus();
  }
}

void YASSFrame::OnClose(wxCloseEvent &event) {
  LOG(INFO) << "Frame is closing";
  event.Skip();  // Destroy() also works here.
#ifdef __APPLE__ /* TODO Destroy cannot help in some cases */
  ::exit(0);
#endif
}

// clang-format off

wxBEGIN_EVENT_TABLE(YASSFrame, wxFrame)
  EVT_MENU(ID_Hello,   YASSFrame::OnHello)
  EVT_MENU(wxID_ABOUT, YASSFrame::OnAbout)
  EVT_DPI_CHANGED(YASSFrame::OnDIPChanged)
  EVT_IDLE(            YASSFrame::OnIdle)
  EVT_CLOSE(YASSFrame::OnClose)
wxEND_EVENT_TABLE()

    // clang-format on
