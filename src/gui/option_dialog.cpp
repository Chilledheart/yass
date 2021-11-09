// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */

#include "gui/option_dialog.hpp"

#include "gui/utils.hpp"

#include "config/config.hpp"

OptionDialog::OptionDialog(wxFrame *parent, const wxString &title,
                           const wxPoint &pos, const wxSize &size)
    : wxDialog(parent, wxID_ANY, title, pos, size,
               wxDEFAULT_DIALOG_STYLE & (~wxCLOSE_BOX) & (~wxMINIMIZE_BOX) &
                   (~wxMAXIMIZE_BOX) & (~wxRESIZE_BORDER)) {
  wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);

  wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);

  wxFlexGridSizer *fgs =
      new wxFlexGridSizer(6, 2, parent->FromDIP(9), parent->FromDIP(25));

  wxStaticText *connecttimeout =
      new wxStaticText(this, -1, wxT("Connect Timeout"));
  wxStaticText *tcpusertimeout =
      new wxStaticText(this, -1, wxT("TCP User Timeout"));
  wxStaticText *lingertimeout =
      new wxStaticText(this, -1, wxT("TCP Linger Timeout"));
  wxStaticText *sendbuffer = new wxStaticText(this, -1, wxT("TCP Send Buffer"));
  wxStaticText *recvbuffer =
      new wxStaticText(this, -1, wxT("TCP Receive Buffer"));

  m_connecttimeout_tc = new wxTextCtrl(this, -1);
  m_tcpusertimeout_tc = new wxTextCtrl(this, -1);
  m_lingertimeout_tc = new wxTextCtrl(this, -1);
  m_sendbuffer_tc = new wxTextCtrl(this, -1);
  m_recvbuffer_tc = new wxTextCtrl(this, -1);

  fgs->Add(connecttimeout);
  fgs->Add(m_connecttimeout_tc, 1, wxEXPAND);
  fgs->Add(tcpusertimeout);
  fgs->Add(m_tcpusertimeout_tc, 1, wxEXPAND);
  fgs->Add(lingertimeout);
  fgs->Add(m_lingertimeout_tc, 1, wxEXPAND);
  fgs->Add(sendbuffer);
  fgs->Add(m_sendbuffer_tc, 1, wxEXPAND);
  fgs->Add(recvbuffer);
  fgs->Add(m_recvbuffer_tc, 1, wxEXPAND);

  fgs->AddGrowableRow(5, 1);
  fgs->AddGrowableCol(1, 1);

  hbox->Add(fgs, 1, wxALL | wxEXPAND, 12);

  vbox->Add(hbox, 1, wxALL | wxEXPAND, 1);

  wxBoxSizer *hbox_buttons = new wxBoxSizer(wxHORIZONTAL);

  m_okay =
      new wxButton(this, wxOK, wxT("OK"), parent->FromDIP(wxPoint(10, 10)));
  m_cancel = new wxButton(this, wxCANCEL, wxT("Cancel"),
                          parent->FromDIP(wxPoint(10, 60)));

  hbox_buttons->Add(m_okay, 1, wxEXPAND, 1);
  hbox_buttons->Add(m_cancel, 1, wxEXPAND, 1);

  vbox->Add(hbox_buttons, 1, wxALL | wxEXPAND, 1);

  this->SetSizer(vbox);

  Connect(wxID_ANY, wxEVT_SHOW, wxShowEventHandler(OptionDialog::OnShow));

  Connect(wxOK, wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(OptionDialog::OnOkay));

  Connect(wxCANCEL, wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(OptionDialog::OnCancel));

#ifdef _WIN32
  SetIcon(wxICON(IDI_ICON1));
#endif
}

void OptionDialog::OnShow(wxShowEvent &WXUNUSED(event)) { OnLoad(); }

void OptionDialog::OnOkay(wxCommandEvent &WXUNUSED(event)) {
  OnSave();
  wxDialog::SetAffirmativeId(wxID_OK);
  wxDialog::AcceptAndClose();
}

void OptionDialog::OnCancel(wxCommandEvent &WXUNUSED(event)) {
  wxDialog::SetAffirmativeId(wxID_CANCEL);
  wxDialog::AcceptAndClose();
}

void OptionDialog::OnLoad() {
  m_connecttimeout_tc->SetValue(std::to_string(FLAGS_timeout));
  m_tcpusertimeout_tc->SetValue(std::to_string(FLAGS_tcp_user_timeout));
  m_lingertimeout_tc->SetValue(std::to_string(FLAGS_so_linger_timeout));
  m_sendbuffer_tc->SetValue(std::to_string(FLAGS_so_snd_buffer));
  m_recvbuffer_tc->SetValue(std::to_string(FLAGS_so_rcv_buffer));
}

void OptionDialog::OnSave() {
  FLAGS_timeout = Utils::Stoi(Utils::ToString(m_connecttimeout_tc->GetValue()));
  FLAGS_tcp_user_timeout =
      Utils::Stoi(Utils::ToString(m_tcpusertimeout_tc->GetValue()));
  FLAGS_so_linger_timeout =
      Utils::Stoi(Utils::ToString(m_lingertimeout_tc->GetValue()));
  FLAGS_so_snd_buffer =
      Utils::Stoi(Utils::ToString(m_sendbuffer_tc->GetValue()));
  FLAGS_so_rcv_buffer =
      Utils::Stoi(Utils::ToString(m_recvbuffer_tc->GetValue()));
}
