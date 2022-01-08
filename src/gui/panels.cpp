// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "gui/panels.hpp"

#include <wx/stattext.h>
#include "gui/utils.hpp"
#include "gui/yass.hpp"

LeftPanel::LeftPanel(wxPanel* parent)
    : wxPanel(parent, -1, wxPoint(-1, -1), wxSize(-1, -1), wxBORDER_THEME) {
  m_parent = parent;
  m_start = new wxButton(this, ID_START, wxT("START"),
#if wxCHECK_VERSION(3, 1, 0)
                         parent->FromDIP(wxPoint(10, 10))
#else
                         wxPoint(10, 10)
#endif
  );
  m_stop = new wxButton(this, ID_STOP, wxT("STOP"),
#if wxCHECK_VERSION(3, 1, 0)
                        parent->FromDIP(wxPoint(10, 60))
#else
                        wxPoint(10, 60)
#endif
  );

  Connect(ID_START, wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(LeftPanel::OnStart));

  Connect(ID_STOP, wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(LeftPanel::OnStop));

  m_stop->Disable();
}

void LeftPanel::OnStart(wxCommandEvent& WXUNUSED(event)) {
  m_start->Disable();
  mApp->OnStart();
}

void LeftPanel::OnStop(wxCommandEvent& WXUNUSED(event)) {
  m_stop->Disable();
  mApp->OnStop();
}

RightPanel::RightPanel(wxPanel* parent)
    : wxPanel(parent,
              wxID_ANY,
              wxDefaultPosition,
#if wxCHECK_VERSION(3, 1, 0)
              parent->FromDIP(wxSize(315, -1)),
#else
              wxSize(315, -1),
#endif
              wxBORDER_THEME) {
  wxString methodStrings[] = {
#define XX(num, name, string) wxT(string),
      CIPHER_METHOD_MAP(XX)
#undef XX
  };
  wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
  wxFlexGridSizer* fgs =
#if wxCHECK_VERSION(3, 1, 0)
      new wxFlexGridSizer(8, 2, parent->FromDIP(9), parent->FromDIP(25));
#else
      new wxFlexGridSizer(8, 2, 9, 25);
#endif

  wxStaticText* serverhost = new wxStaticText(this, -1, wxT("Server Host"));
  wxStaticText* serverport = new wxStaticText(this, -1, wxT("Server Port"));
  wxStaticText* password =
      new wxStaticText(this, -1, wxT("Password"), wxDefaultPosition,
                       wxDefaultSize, wxTE_PASSWORD);
  wxStaticText* method = new wxStaticText(this, -1, wxT("Cipher/Method"));
  wxStaticText* localhost = new wxStaticText(this, -1, wxT("Local Host"));
  wxStaticText* localport = new wxStaticText(this, -1, wxT("Local Port"));

  m_serverhost_tc = new wxTextCtrl(this, -1);
  m_serverport_tc = new wxTextCtrl(this, -1);
  m_password_tc = new wxTextCtrl(this, -1);
  m_method_tc = new wxChoice(this, -1, wxDefaultPosition,
#if wxCHECK_VERSION(3, 1, 0)
                             parent->FromDIP(wxSize(100, -1)),
#else
                             wxSize(100, -1),
#endif
                             WXSIZEOF(methodStrings) - 1, methodStrings + 1, 0);
  m_localhost_tc = new wxTextCtrl(this, -1);
  m_localport_tc = new wxTextCtrl(this, -1);

  wxStaticText* timeout = new wxStaticText(this, -1, wxT("Timeout"));
  wxStaticText* autostart = new wxStaticText(this, -1, wxT("Auto Start"));

  m_timeout_tc = new wxTextCtrl(this, -1);
  m_autostart_cb = new wxCheckBox(this, ID_AUTOSTART, wxT("Enable"));

  fgs->Add(serverhost);
  fgs->Add(m_serverhost_tc, 1, wxEXPAND);
  fgs->Add(serverport);
  fgs->Add(m_serverport_tc, 1, wxEXPAND);
  fgs->Add(password);
  fgs->Add(m_password_tc, 1, wxEXPAND);
  fgs->Add(method);
  fgs->Add(m_method_tc, 1, wxEXPAND);
  fgs->Add(localhost);
  fgs->Add(m_localhost_tc, 1, wxEXPAND);
  fgs->Add(localport);
  fgs->Add(m_localport_tc, 1, wxEXPAND);

  fgs->Add(timeout);
  fgs->Add(m_timeout_tc, 1, wxEXPAND);
  fgs->Add(autostart);
  fgs->Add(m_autostart_cb);

  fgs->AddGrowableRow(7, 1);
  fgs->AddGrowableCol(1, 1);

  hbox->Add(fgs, 1, wxALL | wxEXPAND, 16);
  this->SetSizer(hbox);

  Connect(ID_AUTOSTART, wxEVT_CHECKBOX,
          wxCommandEventHandler(RightPanel::OnCheckedAutoStart));
#if defined(__APPLE__) || defined(_WIN32)
  m_autostart_cb->SetValue(Utils::GetAutoStart());
#else
  m_autostart_cb->Enable(false);
#endif
}

void RightPanel::OnCheckedAutoStart(wxCommandEvent& WXUNUSED(event)) {
#if defined(__APPLE__) || defined(_WIN32)
  Utils::EnableAutoStart(m_autostart_cb->IsChecked());
#endif
}
