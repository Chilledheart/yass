// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "core/address_family.hpp"

#include "core/ip_address.hpp"
#include "core/logging.hpp"
#include "core/pr_util.hpp"

AddressFamily GetAddressFamily(const IPAddress &address) {
  if (address.IsIPv4()) {
    return ADDRESS_FAMILY_IPV4;
  } else if (address.IsIPv6()) {
    return ADDRESS_FAMILY_IPV6;
  } else {
    return ADDRESS_FAMILY_UNSPECIFIED;
  }
}

int ConvertAddressFamily(AddressFamily address_family) {
  switch (address_family) {
  case ADDRESS_FAMILY_UNSPECIFIED:
    return AF_UNSPEC;
  case ADDRESS_FAMILY_IPV4:
    return AF_INET;
  case ADDRESS_FAMILY_IPV6:
    return AF_INET6;
  }
  NOTREACHED();
  return AF_UNSPEC;
}
