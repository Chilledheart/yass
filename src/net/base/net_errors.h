// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_ERRORS_H__
#define NET_BASE_NET_ERRORS_H__

#include <string>

namespace net {

// Error values are negative.
enum Error {
  // No error. Change NetError.template after changing value.
  OK = 0,

#define NET_ERROR(label, value) ERR_ ## label = value,
#include "net/base/net_error_list.h"
#undef NET_ERROR

  // The value of the first certificate error code.
  ERR_CERT_BEGIN = ERR_CERT_COMMON_NAME_INVALID,
};

// Returns a textual representation of the error code for logging purposes.
std::string ErrorToString(int error);

// Same as above, but leaves off the leading "net::".
std::string ErrorToShortString(int error);

// Returns a textual representation of the error code and the extended eror
// code.
std::string ExtendedErrorToString(int error,
                                             int extended_error_code);

// Returns true if |error| is a certificate error code. Note this does not
// include errors for client certificates.
bool IsCertificateError(int error);

// Returns true if |error| is a client certificate authentication error. This
// does not include ERR_SSL_PROTOCOL_ERROR which may also signal a bad client
// certificate.
bool IsClientCertificateError(int error);

// Returns true if |error| is an error from hostname resolution.
bool IsHostnameResolutionError(int error);

// Returns true if |error| means that the request has been blocked.
bool IsRequestBlockedError(int error);

}  // namespace net

#endif  // NET_BASE_NET_ERRORS_H__
