// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "yass_frame.hpp"
#include "panels.hpp"
#include "yass.hpp"
#include <wx/stattext.h>

// clang-format off

wxBEGIN_EVENT_TABLE(YASSFrame, wxFrame)
  EVT_MENU(ID_Hello,   YASSFrame::OnHello)
  EVT_MENU(wxID_EXIT,  YASSFrame::OnExit)
  EVT_MENU(wxID_ABOUT, YASSFrame::OnAbout)
  EVT_IDLE(            YASSFrame::OnIdle)
  EVT_CLOSE(YASSFrame::OnClose)
wxEND_EVENT_TABLE()

    // clang-format on

    YASSFrame::YASSFrame(const wxString &title, const wxPoint &pos,
                         const wxSize &size)
    : wxFrame(NULL, wxID_ANY, title, pos, size,
              (wxDEFAULT_FRAME_STYLE | wxSTAY_ON_TOP) & (~wxMAXIMIZE_BOX) &
                  (~wxRESIZE_BORDER)) {
  wxMenu *menuFile = new wxMenu;
  menuFile->Append(ID_Hello, wxT("&Hello...\tCtrl-H"),
                   wxT("Hell string shown in status bar for this menu item"));
  menuFile->AppendSeparator();
  menuFile->Append(wxID_EXIT);

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

void YASSFrame::Started() {
  UpdateStatus();
  m_rightpanel->m_serverhost_tc->SetEditable(false);
  m_rightpanel->m_serverport_tc->SetEditable(false);
  m_rightpanel->m_password_tc->SetEditable(false);
  // m_rightpanel->m_method_tc->SetEditable(false);
  m_rightpanel->m_localhost_tc->SetEditable(false);
  m_rightpanel->m_localport_tc->SetEditable(false);
  m_leftpanel->m_stop->Enable();
}

void YASSFrame::StartFailed() {
  UpdateStatus();
  m_rightpanel->m_serverhost_tc->SetEditable(true);
  m_rightpanel->m_serverport_tc->SetEditable(true);
  m_rightpanel->m_password_tc->SetEditable(true);
  // m_rightpanel->m_method_tc->SetEditable(true);
  m_rightpanel->m_localhost_tc->SetEditable(true);
  m_rightpanel->m_localport_tc->SetEditable(true);
  m_leftpanel->m_start->Enable();
}

void YASSFrame::Stopped() {
  UpdateStatus();
  m_rightpanel->m_serverhost_tc->SetEditable(true);
  m_rightpanel->m_serverport_tc->SetEditable(true);
  m_rightpanel->m_password_tc->SetEditable(true);
  // m_rightpanel->m_method_tc->SetEditable(true);
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

  SetStatusText(mApp->GetStatus());
}

void YASSFrame::OnHello(wxCommandEvent &WXUNUSED(event)) {
  SetStatusText(wxT("Hello world from YASS!"));
}

void YASSFrame::OnExit(wxCommandEvent &WXUNUSED(event)) {
  LOG(INFO) << "Frame is called to exit";
  wxFrame::Close(true);
}

void YASSFrame::OnAbout(wxCommandEvent &WXUNUSED(event)) {
  wxMessageBox(wxT("This is Yet-Another-Shadow-Socket"), wxT("About YASS"),
               wxOK | wxICON_INFORMATION);
}

void YASSFrame::OnIdle(wxIdleEvent &WXUNUSED(event)) {
  if (mApp->GetState() == YASSApp::STARTED) {
    UpdateStatus();
  }
}

void YASSFrame::OnClose(wxCloseEvent &event) {
  LOG(INFO) << "Frame is closing";
  event.Skip(); // Destroy() also works here.
#ifdef __APPLE__ /* TODO Destroy cannot help in some cases */
  ::exit(0);
#endif
}
