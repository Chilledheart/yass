// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_SYSTEM_EVENT_LOOP_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_SYSTEM_EVENT_LOOP_IMPL_H_

#include <thread>

#include "core/asio.hpp"
#include "core/utils.hpp"

namespace quiche {

thread_local asio::io_context* current_context;

inline void QuicheRunSystemEventLoopIterationImpl() {
  current_context->run_one();
}

class QuicheSystemEventLoopImpl {
 public:
  explicit QuicheSystemEventLoopImpl(std::string context_name) {
    SetThreadName(0, context_name);
    current_context = &io_context_;
  }

 private:
  asio::io_context io_context_;
};

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_SYSTEM_EVENT_LOOP_IMPL_H_
