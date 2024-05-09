// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#include "ios/utils.h"

#include <atomic>
#include <thread>

#include <Network/Network.h>

#include "core/logging.hpp"

static std::atomic<bool> connectedNetwork;
static dispatch_queue_t monitorQueue;
static nw_path_monitor_t monitor;
static std::once_flag initFlag;

// https://forums.developer.apple.com/forums/thread/105822
void initNetworkPathMonitor() {
  DCHECK(!monitor);

  std::call_once(initFlag, []() {
    dispatch_queue_attr_t attrs = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY,
                                                                          DISPATCH_QUEUE_PRIORITY_DEFAULT);
    monitorQueue = dispatch_queue_create("it.gui.yass.network-monitor", attrs);
  });

  // monitor = nw_path_monitor_create_with_type(nw_interface_type_wifi);
  monitor = nw_path_monitor_create();
  nw_path_monitor_set_queue(monitor, monitorQueue);
  if (@available(iOS 14, *)) {
    nw_path_monitor_prohibit_interface_type(monitor, nw_interface_type_loopback);
  }
  nw_path_monitor_set_update_handler(monitor, ^(nw_path_t _Nonnull path) {
    nw_path_status_t status = nw_path_get_status(path);
    // Determine the active interface, but how?
    switch (status) {
      case nw_path_status_invalid: {
        // Network path is invalid
        connectedNetwork = false;
        break;
      }
      case nw_path_status_satisfied: {
        // Network is usable
        connectedNetwork = true;
        break;
      }
      case nw_path_status_satisfiable: {
        // Network may be usable
        connectedNetwork = false;
        break;
      }
      case nw_path_status_unsatisfied: {
        // Network is not usable
        connectedNetwork = false;
        break;
      }
    }
  });

  nw_path_monitor_start(monitor);
}

void deinitNetworkPathMonitor() {
  DCHECK(monitor);

  nw_path_monitor_cancel(monitor);
  monitor = nw_path_monitor_t();
}

bool connectedToNetwork() {
  return connectedNetwork;
}
