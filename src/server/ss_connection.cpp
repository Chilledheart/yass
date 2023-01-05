// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "ss_connection.hpp"

#include <absl/base/attributes.h>

#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/cipher.hpp"

#define MAX_DOWNSTREAM_DEPS 1
#define MAX_UPSTREAM_DEPS 1

namespace ss {

SsConnection::SsConnection(asio::io_context& io_context,
                           const asio::ip::tcp::endpoint& remote_endpoint)
    : Connection(io_context, remote_endpoint),
      state_(),
      resolver_(*io_context_),
      encoder_(new cipher(
          "",
          absl::GetFlag(FLAGS_password),
          static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method)),
          true)),
      decoder_(new cipher("",
                          absl::GetFlag(FLAGS_password),
                          static_cast<enum cipher_method>(
                              absl::GetFlag(FLAGS_cipher_method)))) {}

SsConnection::~SsConnection() {
  VLOG(2) << "Connection (server) " << connection_id() << " freed memory";
}

void SsConnection::start() {
  SetState(state_handshake);
  closed_ = false;
  upstream_writable_ = false;
  downstream_writable_ = true;
  downstream_readable_ = true;
  asio::error_code ec;
  socket_.non_blocking(true, ec);
  ReadHandshake();
}

void SsConnection::close() {
  if (closed_) {
    return;
  }
  VLOG(2) << "Connection (server) " << connection_id()
          << " disconnected with client at stage: "
          << SsConnection::state_to_str(CurrentState())
          << " and data to write: " << downstream_.size();
  asio::error_code ec;
  closed_ = true;
  socket_.close(ec);
  if (ec) {
    VLOG(2) << "close() error: " << ec;
  }
  if (channel_) {
    channel_->close();
  }
  resolver_.cancel();
  auto cb = std::move(disconnect_cb_);
  if (cb) {
    cb();
  }
}

void SsConnection::ReadHandshake() {
  scoped_refptr<SsConnection> self(this);

  socket_.async_read_some(asio::null_buffers(),
      [self](asio::error_code ec, size_t bytes_transferred) {
        std::shared_ptr<IOBuf> cipherbuf{IOBuf::create(SOCKET_BUF_SIZE).release()};
        cipherbuf->reserve(0, SOCKET_BUF_SIZE);
        if (!ec) {
          do {
            bytes_transferred = self->socket_.read_some(mutable_buffer(*cipherbuf), ec);
            if (ec == asio::error::interrupted) {
              continue;
            }
          } while(false);
        }
        if (ec == asio::error::try_again || ec == asio::error::would_block) {
          self->ReadHandshake();
          return;
        }
        if (ec) {
          self->OnDisconnect(ec);
          return;
        }
        cipherbuf->append(bytes_transferred);
        std::shared_ptr<IOBuf> buf = self->DecryptData(cipherbuf);

        DumpHex("HANDSHAKE->", buf.get());

        request_parser::result_type result;
        std::tie(result, std::ignore) = self->request_parser_.parse(
            self->request_, buf->data(), buf->data() + bytes_transferred);

        if (result == request_parser::good) {
          buf->trimStart(self->request_.length());
          buf->retreat(self->request_.length());
          DCHECK_LE(self->request_.length(), bytes_transferred);
          ProcessReceivedData(self, buf, ec, buf->length());
        } else {
          // FIXME better error code?
          ec = asio::error::connection_refused;
          self->OnDisconnect(ec);
        }
      });
}

void SsConnection::ResolveDns(std::shared_ptr<IOBuf> buf) {
  scoped_refptr<SsConnection> self(this);
  resolver_.async_resolve(
      self->request_.domain_name(), std::to_string(self->request_.port()),
      [self, buf](asio::error_code ec,
                  asio::ip::tcp::resolver::results_type results) {
        // Get a list of endpoints corresponding to the SOCKS 5 domain name.
        if (!ec) {
          self->remote_endpoint_ = results->endpoint();
          VLOG(3) << "Connection (server) " << self->connection_id()
                  << " resolved address: " << self->request_.domain_name();
          self->SetState(state_stream);
          self->OnConnect();
          ProcessReceivedData(self, buf, ec, buf->length());
        } else {
          self->OnDisconnect(ec);
        }
      });
}

void SsConnection::ReadStream() {
  scoped_refptr<SsConnection> self(this);
  downstream_read_inprogress_ = true;

  socket_.async_read_some(asio::null_buffers(),
      [self](asio::error_code ec,
             std::size_t bytes_transferred) {
        self->downstream_read_inprogress_ = false;
        std::shared_ptr<IOBuf> cipherbuf{IOBuf::create(SOCKET_BUF_SIZE).release()};
        cipherbuf->reserve(0, SOCKET_BUF_SIZE);
        if (!ec) {
          do {
            bytes_transferred = self->socket_.read_some(mutable_buffer(*cipherbuf), ec);
            if (ec == asio::error::interrupted) {
              continue;
            }
          } while(false);
        }
        if (ec == asio::error::try_again || ec == asio::error::would_block) {
          self->ReadStream();
          return;
        }
        if (bytes_transferred || ec) {
          cipherbuf->append(bytes_transferred);
          std::shared_ptr<IOBuf> buf;
          if (!ec) {
            buf = self->DecryptData(cipherbuf);
            bytes_transferred = buf->length();
          }
          ProcessReceivedData(self, buf, ec, bytes_transferred);
          return;
        }
        LOG(FATAL) << "bytes_transferred is zero when ec is success";
      });
}

void SsConnection::WriteStream(std::shared_ptr<IOBuf> buf) {
  scoped_refptr<SsConnection> self(this);
  DCHECK(downstream_writable_);
  downstream_writable_ = false;
  asio::async_write(socket_, const_buffer(*buf),
      [self, buf](asio::error_code error, size_t bytes_transferred) {
        return SsConnection::ProcessSentData(self, buf, error,
                                             bytes_transferred);
      });
}

void SsConnection::ProcessReceivedData(scoped_refptr<SsConnection> self,
                                       std::shared_ptr<IOBuf> buf,
                                       asio::error_code ec,
                                       size_t bytes_transferred) {
  VLOG(3) << "Connection (server) " << self->connection_id()
          << " received data: " << bytes_transferred << " bytes"
          << " ec: " << ec;

  self->rbytes_transferred_ += bytes_transferred;

  if (buf) {
    DCHECK_LE(bytes_transferred, buf->length());
  }

  if (!ec) {
    switch (self->CurrentState()) {
      case state_handshake:
        if (self->request_.address_type() == domain) {
          self->ResolveDns(buf);
          return;
        }
        self->remote_endpoint_ = self->request_.endpoint();
        self->SetState(state_stream);
        self->OnConnect();
        DCHECK_EQ(buf->length(), bytes_transferred);
        ABSL_FALLTHROUGH_INTENDED;
        /* fall through */
      case state_stream:
        if (bytes_transferred) {
          self->OnStreamRead(buf);
        }
        if (self->downstream_readable_) {
          self->ReadStream();  // continously read
        }
        break;
      case state_error:
        ec = std::make_error_code(std::errc::bad_message);
        break;
    };
  }
#if 1
  // Silence Read EOF error triggered by upstream disconnection
  if (ec == asio::error::eof && self->channel_ && self->channel_->eof()) {
    return;
  }
#endif
  if (ec) {
    self->SetState(state_error);
    self->OnDisconnect(ec);
  }
};

void SsConnection::ProcessSentData(scoped_refptr<SsConnection> self,
                                   std::shared_ptr<IOBuf> buf,
                                   asio::error_code ec,
                                   size_t bytes_transferred) {
  VLOG(3) << "Connection (server) " << self->connection_id()
          << " sent data: " << bytes_transferred << " bytes"
          << " ec: " << ec << " and data to write: " << self->downstream_.size();

  self->wbytes_transferred_ += bytes_transferred;

  if (buf) {
    DCHECK_LE(bytes_transferred, buf->length());
  }

  if (!ec) {
    switch (self->CurrentState()) {
      case state_stream:
        self->OnStreamWrite(buf);
        break;
      case state_handshake:
      case state_error:
        ec = std::make_error_code(std::errc::bad_message);
        break;
    }
  }

  if (ec) {
    self->SetState(state_error);
    self->OnDisconnect(ec);
  }
};

void SsConnection::OnConnect() {
  VLOG(2) << "Connection (server) " << connection_id()
          << " established connection with: " << endpoint_
          << " remote: " << remote_endpoint_;
  channel_ = std::make_unique<stream>(*io_context_, remote_endpoint_, this);
  channel_->connect();
}

void SsConnection::OnStreamRead(std::shared_ptr<IOBuf> buf) {
  // queue limit to downstream read
  if (upstream_.size() >= MAX_UPSTREAM_DEPS && downstream_readable_) {
    DisableStreamRead();
  }

  OnUpstreamWrite(buf);
}

void SsConnection::OnStreamWrite(std::shared_ptr<IOBuf> buf) {
  DCHECK(!downstream_writable_);
  DCHECK(!downstream_.empty() && downstream_[0] == buf);
  downstream_.pop_front();
  downstream_writable_ = true;

  /* recursively send the remainings */
  while (downstream_.size()) {
    DCHECK(downstream_writable_);
    asio::error_code ec;
    std::shared_ptr<IOBuf> buf = downstream_.front();
    size_t written;
    do {
      written = socket_.write_some(const_buffer(*buf), ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    VLOG(3) << "Connection (server) " << connection_id()
            << " sent data (pipe): " << written << " bytes"
            << " ec: " << ec << " and data to write: " << downstream_.size();
    buf->trimStart(written);
    wbytes_transferred_ += written;
    if (buf->empty())
      downstream_.pop_front();
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      break;
    }
    if (ec) {
      OnDisconnect(ec);
      return;
    }
  }

  // if in_progress
  while (upstream_readable_) {
    asio::error_code ec;
    bool eof = false;
    std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
    buf->reserve(0, SOCKET_BUF_SIZE);
    size_t read;
    do {
      read = channel_->read_some(buf, ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    buf->append(read);
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      eof = true;
    } else if (ec) {
      /* safe to return, channel will handle this error */
      return;
    }
    if (!read) {
      break;
    }
    DCHECK(downstream_writable_);
    VLOG(3) << "Connection (server) " << connection_id()
            << " upstream: received reply (pipe): " << buf->length() << " bytes.";
    buf = EncryptData(buf);
    ec = asio::error_code();
    size_t written;
    do {
      written = socket_.write_some(const_buffer(*buf), ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    VLOG(3) << "Connection (server) " << connection_id()
            << " sent data (pipe): " << written << " bytes"
            << " ec: " << ec << " and bytes to write: "
            << (buf->length() - written);
    buf->trimStart(written);
    wbytes_transferred_ += written;
    // continue to resume
    if (!buf->empty()) {
      VLOG(3) << "Connection (server) " << connection_id()
              << " partially sent data (pipe): " << written << " bytes";
      downstream_.push_back(buf);
      WriteStream(downstream_[0]);
    }
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      break;
    }
    if (ec) {
      OnDisconnect(ec);
      return;
    }
    if (eof || !buf->empty()) {
      break;
    }
  }

  OnDownstreamWriteFlush();

  /* shutdown the socket if upstream is eof and all remaining data sent */
  if (channel_->eof() && downstream_.empty()) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " last data sent: shutting down";
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    return;
  }

  /* disable queue limit to re-enable upstream read */
  if (downstream_.size() < MAX_DOWNSTREAM_DEPS && !upstream_readable_) {
    upstream_readable_ = true;
    channel_->enable_read();
  }
}

void SsConnection::EnableStreamRead() {
  if (!downstream_readable_) {
    downstream_readable_ = true;
    if (!downstream_read_inprogress_) {
      ReadStream();
    }
  }
}

void SsConnection::DisableStreamRead() {
  downstream_readable_ = false;
}

void SsConnection::OnDisconnect(asio::error_code ec) {
  VLOG(2) << "Connection (server) " << connection_id()
          << " lost connection with: " << endpoint_ << " due to " << ec
          << " and data to write: " << downstream_.size();
  close();
}

void SsConnection::OnDownstreamWriteFlush() {
  if (!downstream_.empty()) {
    OnDownstreamWrite(nullptr);
  }
}

void SsConnection::OnDownstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    downstream_.push_back(EncryptData(buf));
  }

  if (!downstream_.empty() && downstream_writable_) {
    WriteStream(downstream_[0]);
  }
}

void SsConnection::OnUpstreamWriteFlush() {
  std::shared_ptr<IOBuf> buf{IOBuf::create(0).release()};
  OnUpstreamWrite(buf);
}

void SsConnection::OnUpstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    upstream_.push_back(buf);
  }
  if (!upstream_.empty() && upstream_writable_) {
    upstream_writable_ = false;
    channel_->start_write(upstream_[0]);
  }
}

void SsConnection::connected() {
  VLOG(2) << "Connection (server) " << connection_id()
          << " remote: established upstream connection with: " << remote_endpoint_;
  upstream_readable_ = true;
  upstream_writable_ = true;
  channel_->start_read();
  OnUpstreamWriteFlush();
}

void SsConnection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (server) " << connection_id()
          << " upstream: received reply: " << buf->length() << " bytes.";

  // queue limit to upstream read
  if (downstream_.size() >= MAX_DOWNSTREAM_DEPS && upstream_readable_) {
    upstream_readable_ = false;
    channel_->disable_read();
  }

  OnDownstreamWrite(buf);
}

void SsConnection::sent(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (server) " << connection_id()
          << " upstream: sent request: " << buf->length() << " bytes.";
  DCHECK(!upstream_.empty() && upstream_[0] == buf);
  upstream_.pop_front();

  upstream_writable_ = true;

  /* recursively send the remainings */
  while (upstream_.size()) {
    asio::error_code ec;
    std::shared_ptr<IOBuf> buf = upstream_.front();
    size_t written;
    do {
      written = channel_->write_some(buf, ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    VLOG(3) << "Connection (server) " << connection_id()
            << " upstream: sent request (pipe): " << written << " bytes"
            << " ec: " << ec << " and data to write: " << upstream_.size();
    buf->trimStart(written);
    wbytes_transferred_ += written;
    if (buf->empty())
      upstream_.pop_front();
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      break;
    }
    if (ec) {
      OnDisconnect(ec);
      return;
    }
  }
  // if in_progress
  while (downstream_readable_) {
    asio::error_code ec;
    bool eof = false;
    std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
    buf->reserve(0, SOCKET_BUF_SIZE);
    size_t read;
    do {
      read = socket_.read_some(mutable_buffer(*buf), ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    buf->append(read);
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      eof = true;
    } else if (ec) {
      /* safe to return, socket will handle this error */
      return;
    }
    if (!read) {
      break;
    }
    VLOG(3) << "Connection (server) " << connection_id()
            << " received data (pipe): " << buf->length() << " bytes.";
    buf = DecryptData(buf);
    ec = asio::error_code();
    size_t written;
    do {
      written = channel_->write_some(buf, ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    VLOG(3) << "Connection (server) " << connection_id()
            << " upstream: sent request (pipe): " << written << " bytes"
            << " ec: " << ec << " and data to write: "
            << (buf->length() - written);
    buf->trimStart(written);
    wbytes_transferred_ += written;
    if (!buf->empty()) {
      VLOG(3) << "Connection (server) " << connection_id()
              << " upstream: partially sent data (pipe): " << written << " bytes";
      upstream_.push_back(buf);
      upstream_writable_ = false;
      channel_->start_write(upstream_[0]);
    }
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      break;
    }
    if (ec) {
      OnDisconnect(ec);
      return;
    }
    if (eof || !buf->empty()) {
      break;
    }
  }
  OnUpstreamWriteFlush();

  if (upstream_.size() < MAX_UPSTREAM_DEPS && !downstream_readable_) {
    EnableStreamRead();
  }
}

void SsConnection::disconnected(asio::error_code ec) {
  VLOG(2) << "Connection (server) " << connection_id()
          << " upstream: lost connection with: " << remote_endpoint_
          << " due to " << ec
          << " and data to write: " << downstream_.size();
  upstream_readable_ = false;
  upstream_writable_ = false;
  channel_->close();
  /* delay the socket's close because downstream is buffered */
  if (downstream_.empty()) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " upstream: last data sent: shutting down";
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
  } else {
    socket_.shutdown(asio::ip::tcp::socket::shutdown_receive, ec);
  }
}

std::shared_ptr<IOBuf> SsConnection::DecryptData(
    std::shared_ptr<IOBuf> cipherbuf) {
  std::shared_ptr<IOBuf> plainbuf = IOBuf::create(cipherbuf->length());
  plainbuf->reserve(0, cipherbuf->length());

  DumpHex("ERead->", cipherbuf.get());
  decoder_->decrypt(cipherbuf.get(), &plainbuf);
  DumpHex("PRead->", plainbuf.get());
  return plainbuf;
}

std::shared_ptr<IOBuf> SsConnection::EncryptData(std::shared_ptr<IOBuf> plainbuf) {
  std::shared_ptr<IOBuf> cipherbuf = IOBuf::create(plainbuf->length() + 100);
  cipherbuf->reserve(0, plainbuf->length() + 100);

  DumpHex("PWrite->", plainbuf.get());
  encoder_->encrypt(plainbuf.get(), &cipherbuf);
  DumpHex("EWrite->", cipherbuf.get());
  return cipherbuf;
}

}  // namespace ss
