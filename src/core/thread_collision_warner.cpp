// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/thread_collision_warner.hpp"
#include <atomic>
#include <ostream>
#include <thread>
#include "core/logging.hpp"

void DCheckAsserter::warn() {
  NOTREACHED() << "Thread Collision";
}
