//
// ip_endpoint.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_IP_ENDPOINT
#define H_IP_ENDPOINT

#include "core/ip_address.hpp"
#include "core/address_family.hpp"
#include <stdint.h>
#include <string>
#include <tuple>

class IPEndPoint {
 public:
  IPEndPoint();
  ~IPEndPoint();
  IPEndPoint(const IPAddress& address, uint16_t port);
  IPEndPoint(const IPEndPoint& endpoint);

  const IPAddress& address() const { return address_; }
  uint16_t port() const { return port_; }

  // Returns AddressFamily of the address.
  AddressFamily GetFamily() const;

  // Returns the sockaddr family of the address, AF_INET or AF_INET6.
  int GetSockAddrFamily() const;

  // Convert to a provided sockaddr struct.
  // |address| is the sockaddr to copy into.  Should be at least
  //    sizeof(struct sockaddr_storage) bytes.
  // |address_length| is an input/output parameter.  On input, it is the
  //    size of data in |address| available.  On output, it is the size of
  //    the address that was copied into |address|.
  // Returns true on success, false on failure.
  bool ToSockAddr(struct sockaddr* address, int* address_length) const
      ;

  // Convert from a sockaddr struct.
  // |address| is the address.
  // |address_length| is the length of |address|.
  // Returns true on success, false on failure.
  bool FromSockAddr(const struct sockaddr* address,
                    int address_length);

  // Returns value as a string (e.g. "127.0.0.1:80"). Returns the empty string
  // when |address_| is invalid (the port will be ignored).
  std::string ToString() const;

  // As above, but without port. Returns the empty string when address_ is
  // invalid.
  std::string ToStringWithoutPort() const;

  bool operator<(const IPEndPoint& that) const;
  bool operator==(const IPEndPoint& that) const;
  bool operator!=(const IPEndPoint& that) const;

 private:
  IPAddress address_;
  uint16_t port_;
};

#endif // H_IP_ENDPOINT
