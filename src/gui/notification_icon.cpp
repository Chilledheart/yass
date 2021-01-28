// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */

#include "gui/notification_icon.hpp"

#include "gui/yass_frame.hpp"
#include <wx/menu.h>

const int NotificationIcon::PopupExitID = wxID_HIGHEST + 1;

NotificationIcon::NotificationIcon() : wxTaskBarIcon() { mainFrame = NULL; }

NotificationIcon::~NotificationIcon() {}

void NotificationIcon::SetMainFrame(YASSFrame *frame) { mainFrame = frame; }

void NotificationIcon::OnLeftDoubleClick(wxTaskBarIconEvent &event) {
  if (mainFrame) {
    if (!!mainFrame->IsShown())
      mainFrame->Raise();
    mainFrame->Show(!mainFrame->IsShown());
  }
}

wxMenu *NotificationIcon::CreatePopupMenu() {
  wxMenu *popup = new wxMenu;

  // popup->AppendSeparator();
  popup->Append(PopupExitID, wxT("E&xit"));
  return popup;
}

void NotificationIcon::OnQuit(wxCommandEvent &WXUNUSED(event)) {
  RemoveIcon();
  if (mainFrame)
    mainFrame->Close(true);
}

// clang-format off
BEGIN_EVENT_TABLE(NotificationIcon, wxTaskBarIcon)
  EVT_TASKBAR_LEFT_DCLICK(NotificationIcon::OnLeftDoubleClick)
  EVT_MENU(PopupExitID, NotificationIcon::OnQuit)
END_EVENT_TABLE()
// clang-format on