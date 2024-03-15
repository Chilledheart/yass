// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/resolver.hpp"

#include <absl/flags/flag.h>

ABSL_FLAG(std::string, doh_url, "", "Resolve host names over DoH");

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
    doh_url_ = absl::GetFlag(FLAGS_doh_url);
  }

}  // namespace net
