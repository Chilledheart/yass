// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/protocol.hpp"

#include <string_view>

using std::string_view_literals::operator""sv;

namespace net {

NextProto NextProtoFromString(std::string_view proto_string) {
  if (proto_string == "http/1.1"sv) {
    return kProtoHTTP11;
  }
  if (proto_string == "h2"sv) {
    return kProtoHTTP2;
  }
  if (proto_string == "quic"sv || proto_string == "hq"sv) {
    return kProtoQUIC;
  }

  return kProtoUnknown;
}

std::string_view NextProtoToString(NextProto next_proto) {
  switch (next_proto) {
    case kProtoHTTP11:
      return "http/1.1"sv;
    case kProtoHTTP2:
      return "h2"sv;
    case kProtoQUIC:
      return "quic"sv;
    case kProtoUnknown:
      break;
  }
  return "unknown";
}

}  // namespace net
