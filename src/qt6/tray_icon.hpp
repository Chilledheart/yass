// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#include <QSystemTrayIcon>

class TrayIcon : public QSystemTrayIcon {
  Q_OBJECT

 public:
  TrayIcon(QObject* parent);

 private slots:
  void onActivated(QSystemTrayIcon::ActivationReason);
  void OnOption();
};
#endif  // TRAY_ICON_H
