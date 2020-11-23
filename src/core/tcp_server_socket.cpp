// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "core/tcp_server_socket.hpp"
#include "core/logging.hpp"

TCPServerSocket::TCPServerSocket(PRFileDesc *socket)
    : socket_(socket), pending_accept_(false) {}

TCPServerSocket::~TCPServerSocket() = default;

int TCPServerSocket::Listen(const IPEndPoint &address, int backlog) {
  PRStatus result;

  socket_ = PR_OpenTCPSocket(address.GetFamily());
  if (!socket_) {
    goto err_and_failure;
  }

#if 0
  result = socket_->SetDefaultOptionsForServer();
  if (result != OK) {
    goto err_and_failure;
  }
#endif

  result = PR_Bind(socket_, (const PNetAddr *)&address.bytes().data());
  if (result != PR_SUCCESS) {
    goto err_and_failure;
  }

  result = PR_Listen(socket_, backlog);
  if (result != PR_SUCCESS) {
    goto err_and_failure;
  }

  return 0;

err_and_failure:
  PR_Close(socket_);
  return -1;
}

int TCPServerSocket::GetLocalAddress(IPEndPoint *address) const {
  PNetAddr nAddr;
  if (PR_GetSockName(socket_, &nAddr) == PR_SUCCESS) {
    *address = IPAddress(&nAddr, PNetAddrGetLen(&nAddr));
    return 0;
  }
  return -1;
}

int TCPServerSocket::Accept(std::unique_ptr<StreamSocket> *socket,
                            CompletionOnceCallback callback) {
  DCHECK(socket);
  DCHECK(callback);

  if (pending_accept_) {
    NOTREACHED();
    return -1;
  }

  // It is safe to use base::Unretained(this). |socket_| is owned by this class,
  // and the callback won't be run after |socket_| is destroyed.
  CompletionOnceCallback accept_callback = std::bind(
      &TCPServerSocket::OnAcceptCompleted, this, socket, std::move(callback));

  PNetAddr nAddr;
  accepted_socket_ = PR_Accept(socket_, &nAddr, 0);
  if (accepted_socket_ != nullptr) {
    // |accept_callback| won't be called so we need to run
    // ConvertAcceptedSocket() ourselves in order to do the conversion from
    // |accepted_socket_| to |socket|.
    result = ConvertAcceptedSocket(result, socket);
  } else {
    pending_accept_ = true;
  }

  return -1;
}

int TCPServerSocket::ConvertAcceptedSocket(
    int result, std::unique_ptr<StreamSocket> *output_accepted_socket) {
  // Make sure the TCPSocket object is destroyed in any case.
  std::unique_ptr<TCPSocket> temp_accepted_socket(std::move(accepted_socket_));
  if (result != OK)
    return result;

  output_accepted_socket->reset(
      new TCPClientSocket(std::move(temp_accepted_socket), accepted_address_));

  return OK;
}

void TCPServerSocket::OnAcceptCompleted(
    std::unique_ptr<StreamSocket> *output_accepted_socket,
    CompletionOnceCallback forward_callback, int result) {
  result = ConvertAcceptedSocket(result, output_accepted_socket);
  pending_accept_ = false;
  std::move(forward_callback)(result);
}
