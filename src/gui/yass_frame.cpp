//
// yass_frame.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "yass_frame.hpp"
#include "panels.hpp"
#include "yass.hpp"
#include <wx/stattext.h>

// clang-format off

wxBEGIN_EVENT_TABLE(YASSFrame, wxFrame)
  EVT_MENU(ID_Hello, YASSFrame::OnHello)
  EVT_MENU(wxID_EXIT, YASSFrame::OnExit)
  EVT_MENU(wxID_ABOUT, YASSFrame::OnAbout)
wxEND_EVENT_TABLE()

    // clang-format on

    YASSFrame::YASSFrame(const wxString &title, const wxPoint &pos,
                         const wxSize &size)
    : wxFrame(NULL, wxID_ANY, title, pos, size) {
  wxMenu *menuFile = new wxMenu;
  menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
                   "Hell string shown in status bar for this menu item");
  menuFile->AppendSeparator();
  menuFile->Append(wxID_EXIT);

  wxMenu *menuHelp = new wxMenu;
  menuHelp->Append(wxID_ABOUT);

  wxMenuBar *menuBar = new wxMenuBar;
  menuBar->Append(menuFile, "&File");
  menuBar->Append(menuHelp, "&Help");

  SetMenuBar(menuBar);

  CreateStatusBar();
  SetStatusText("READY");

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
  m_leftpanel->m_stop->Enable();
}

void YASSFrame::StartFailed() {
  UpdateStatus();
  m_leftpanel->m_start->Enable();
}

void YASSFrame::Stopped() {
  UpdateStatus();
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

void YASSFrame::OnExit(wxCommandEvent &event) { Close(true); }

void YASSFrame::OnAbout(wxCommandEvent &event) {
  wxMessageBox("This is Yet-Another-Shadow-Socket", "About YASS",
               wxOK | wxICON_INFORMATION);
}

void YASSFrame::OnHello(wxCommandEvent &event) {
  wxLogMessage("Hello world from wxWidgets!");
}
