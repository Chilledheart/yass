// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "qt6/tray_icon.hpp"

#include <QAction>
#include <QMenu>

#include "qt6/option_dialog.hpp"
#include "qt6/yass.hpp"
#include "qt6/yass_window.hpp"

TrayIcon::TrayIcon(QObject* parent) : QSystemTrayIcon(parent) {
  setIcon(QIcon(":/res/images/yass.png"));

  // create action
  QAction* option_action = new QAction(tr("Option"), this);
  connect(option_action, &QAction::triggered, this, &TrayIcon::OnOption);

  QAction* exit_action = new QAction(tr("Exit"), this);
  connect(exit_action, &QAction::triggered, this, [&]() { App()->quit(); });

  QMenu* menu = new QMenu(tr("File"));
  menu->addAction(option_action);
  menu->addSeparator();
  menu->addAction(exit_action);

  setContextMenu(menu);

  // connect signal
  connect(this, &TrayIcon::activated, this, &TrayIcon::onActivated);
}

void TrayIcon::OnOption() {
  App()->mainWindow()->showWindow();
  OptionDialog dialog(App()->mainWindow());
  dialog.exec();
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason) {
  switch (reason) {
    case QSystemTrayIcon::Trigger:  // single click
    case QSystemTrayIcon::MiddleClick:
    case QSystemTrayIcon::DoubleClick:
      App()->mainWindow()->showWindow();
      break;
    default:
      break;
  }
}
