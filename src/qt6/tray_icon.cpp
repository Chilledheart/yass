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
  QAction* show_action = new QAction(tr("Show"), this);
  connect(show_action, &QAction::triggered, this, &TrayIcon::OnShow);

  QAction* option_action = new QAction(tr("Option"), this);
  connect(option_action, &QAction::triggered, this, &TrayIcon::OnOption);

  QAction* exit_action = new QAction(tr("Exit"), this);
  connect(exit_action, &QAction::triggered, this, [&]() { App()->quit(); });

  QMenu* menu = new QMenu(tr("File"));
  menu->addAction(show_action);
  menu->addAction(option_action);
  menu->addSeparator();
  menu->addAction(exit_action);

  setContextMenu(menu);

  // connect signal
  connect(this, &TrayIcon::activated, this, &TrayIcon::OnActivated);
}

void TrayIcon::OnActivated(QSystemTrayIcon::ActivationReason reason) {
  switch (reason) {
    case QSystemTrayIcon::Trigger:  // single click
    case QSystemTrayIcon::MiddleClick:
    case QSystemTrayIcon::DoubleClick:
      OnShow();
      break;
    default:
      break;
  }
}

void TrayIcon::OnShow() {
  App()->mainWindow()->showWindow();
}

void TrayIcon::OnOption() {
  App()->mainWindow()->showWindow();
  OptionDialog dialog(App()->mainWindow());
  dialog.exec();
}
