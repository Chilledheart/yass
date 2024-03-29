// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/resolver.hpp"
#include "config/config.hpp"

namespace net {

Resolver::Resolver(asio::io_context& io_context)
    : io_context_(io_context),
      doh_resolver_(nullptr),
#ifdef HAVE_C_ARES
      resolver_(nullptr)
#else
      resolver_(io_context)
#endif
{
}

int Resolver::Init() {
  doh_url_ = absl::GetFlag(FLAGS_doh_url);
  if (!doh_url_.empty()) {
    doh_resolver_ = DoHResolver::Create(io_context_);
    return doh_resolver_->Init(doh_url_, 10000);
  }
#ifdef HAVE_C_ARES
  resolver_ = CAresResolver::Create(io_context_);
  return resolver_->Init(5000);
#else
  return 0;
#endif
}

}  // namespace net
