// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "core/server_socket.hpp"
#include "core/ip_address.hpp"
#include "core/ip_endpoint.hpp"

ServerSocket::ServerSocket() = default;

ServerSocket::~ServerSocket() = default;

#if 0
int ServerSocket::ListenWithAddressAndPort(const std::string& address_string,
                                           uint16_t port,
                                           int backlog) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(address_string)) {
    return ERR_ADDRESS_INVALID;
  }

  return Listen(IPEndPoint(ip_address, port), backlog);
}

#endif
