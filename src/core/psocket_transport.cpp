//
// psocket_transport.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "core/psocket_transport.hpp"

bool PSocketTransport::bind(const PNetAddr &addr) {
  PRStatus status = PR_Bind(fd, &prAddr);
  return status.ok();
}

PSocketTransport::PSocketTransport()
    : host_(), port_(), peer_addr_(), self_addr_(), fd_(), rByteCount_(),
      wByteCount_() {}

bool PSocket::Init(const char **socketTypes, uint32_t typeCount,
                   const std::string &host, uint16_t port,
                   const std::string &hostRoute, uint16_t portRoute) {}

bool PSocket::InitWithConnectedSocket(PRFileDesc *socketFD,
                                      const PNetAddr *addr) {}

bool PSocket::InitiateSocket() {
  PRStatus status;
  // Make the socket non-blocking...
  PRSocketOptionData opt;
  opt.option = PR_SockOpt_Nonblocking;
  opt.value.non_blocking = true;
  status = PR_SetSocketOption(fd, &opt);
  NS_ASSERTION(status == PR_SUCCESS, "unable to make socket non-blocking");
}
