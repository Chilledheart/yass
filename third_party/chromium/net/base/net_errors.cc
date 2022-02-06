// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"

#include <string>

#include "core/check_op.hpp"
#include "core/logging.hpp"
#include "core/check.hpp"
#include "quic/core/quic_error_codes.h"

namespace net {

// Validate all error values in net_error_list.h are negative.
#define NET_ERROR(label, value) \
  static_assert(value < 0, "ERR_" #label " should be negative");
#include "net/base/net_error_list.h"
#undef NET_ERROR

std::string ErrorToString(int error) {
  return "net::" + ErrorToShortString(error);
}

std::string ExtendedErrorToString(int error, int extended_error_code) {
  if (error == ERR_QUIC_PROTOCOL_ERROR && extended_error_code != 0) {
    return std::string("net::ERR_QUIC_PROTOCOL_ERROR.") +
           QuicErrorCodeToString(
               static_cast<quic::QuicErrorCode>(extended_error_code));
  }
  return ErrorToString(error);
}

std::string ErrorToShortString(int error) {
  if (error == OK)
    return "OK";

  const char* error_string;
  switch (error) {
#define NET_ERROR(label, value) \
  case ERR_ ## label: \
    error_string = # label; \
    break;
#include "net/base/net_error_list.h"
#undef NET_ERROR
  default:
    NOTREACHED();
    error_string = "<unknown>";
  }
  return std::string("ERR_") + error_string;
}

bool IsCertificateError(int error) {
  // Certificate errors are negative integers from net::ERR_CERT_BEGIN
  // (inclusive) to net::ERR_CERT_END (exclusive) in *decreasing* order.
  // ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN is currently an exception to this
  // rule.
  return (error <= ERR_CERT_BEGIN && error > ERR_CERT_END) ||
         (error == ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN);
}

bool IsClientCertificateError(int error) {
  switch (error) {
    case ERR_BAD_SSL_CLIENT_AUTH_CERT:
    case ERR_SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED:
    case ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY:
    case ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED:
    case ERR_SSL_CLIENT_AUTH_NO_COMMON_ALGORITHMS:
      return true;
    default:
      return false;
  }
}

bool IsHostnameResolutionError(int error) {
  DCHECK_NE(ERR_NAME_RESOLUTION_FAILED, error);
  return error == ERR_NAME_NOT_RESOLVED;
}

bool IsRequestBlockedError(int error) {
  switch (error) {
    case ERR_BLOCKED_BY_CLIENT:
    case ERR_BLOCKED_BY_ADMINISTRATOR:
    case ERR_BLOCKED_BY_CSP:
      return true;
    default:
      return false;
  }
}

}  // namespace net
