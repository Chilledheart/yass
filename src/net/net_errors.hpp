// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_ERRORS_H__
#define NET_BASE_NET_ERRORS_H__

namespace net {

// Error values are negative.
enum Error {
  // No error. Change NetError.template after changing value.
  OK = 0,

#define NET_ERROR(label, value) ERR_ ## label = value,
#include "net/net_error_list.hpp"
#undef NET_ERROR

  // The value of the first certificate error code.
  ERR_CERT_BEGIN = ERR_CERT_COMMON_NAME_INVALID,
};

}  // namespace net

#endif  // NET_BASE_NET_ERRORS_H__
