// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_SYSTEM_EVENT_LOOP_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_SYSTEM_EVENT_LOOP_IMPL_H_

#include <thread>

#include "core/asio.hpp"
#include "core/utils.hpp"

thread_local asio::io_context* current_context;

inline void QuicRunSystemEventLoopIterationImpl() {
  current_context->run_one();
}

class QuicSystemEventLoopImpl {
 public:
  QuicSystemEventLoopImpl(std::string context_name) {
    SetThreadName(0, context_name);
    current_context = &io_context_;
  }

 private:
  asio::io_context io_context_;
};

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_SYSTEM_EVENT_LOOP_IMPL_H_
