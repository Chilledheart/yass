// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */

#ifndef NOTIFICATIONICON_H
#define NOTIFICATIONICON_H

#include <wx/taskbar.h>

class YASSFrame;

class NotificationIcon : public wxTaskBarIcon {
public:
  NotificationIcon();
  ~NotificationIcon();

  void SetMainFrame(YASSFrame *frame);

  void OnLeftDoubleClick(wxTaskBarIconEvent &event);
  void OnQuit(wxCommandEvent &event);
  virtual wxMenu *CreatePopupMenu();

protected:
  static const int PopupExitID;

  YASSFrame *mainFrame;

private:
  DECLARE_EVENT_TABLE()
};

#endif