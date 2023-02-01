// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "core/c-ares.hpp"
class CAresLibLoader {
 public:
  CAresLibLoader() {
    ::ares_library_init(ARES_LIB_INIT_ALL);
    LOG(INFO) << "C-Ares Loaded";
  }
  ~CAresLibLoader() {
    ::ares_library_cleanup();
    LOG(INFO) << "C-Ares Unloaded";
  }
};

CAresResolver::CAresResolver(asio::io_context &io_context) :
  io_context_(io_context), resolve_timer_(io_context) {
  static CAresLibLoader loader;
}

CAresResolver::~CAresResolver() {
  Destroy();
  VLOG(2) << "c-ares resolver freed memory";
}

int CAresResolver::Init(int timeout_ms, int retries) {
  ares_opts_.lookups = lookups_;
  ares_opts_.timeout = timeout_ms_ = timeout_ms;
  ares_opts_.tries = retries;
  ares_opts_.sock_state_cb_data = this;
  ares_opts_.sock_state_cb = &CAresResolver::OnSockState;
  int ret = ::ares_init_options(&channel_, &ares_opts_,
                                ARES_OPT_TIMEOUTMS | ARES_OPT_TRIES |
                                ARES_OPT_SOCK_STATE_CB | ARES_OPT_LOOKUPS);
  if (ret) {
    LOG(WARNING) << "ares_init_options failure: " << ares_strerror(ret);
  }
  return ret;
}

void CAresResolver::Destroy() {
  ::ares_destroy(channel_);
}

void CAresResolver::OnSockState(void *arg, fd_t fd, int readable, int writable) {
  auto self = scoped_refptr(reinterpret_cast<CAresResolver*>(arg));
  auto iter = self->fd_map_.find(fd);
  if (!readable && !writable) {
    if (iter != self->fd_map_.end()) {
      iter->second->read_enable = false;
      iter->second->write_enable = false;
      self->fd_map_.erase(iter);
    }
    return;
  }
  if (iter == self->fd_map_.end()) {
    auto value = std::make_pair(fd, ResolverPerContext::Create(self->io_context_, fd));
    iter = self->fd_map_.insert(value).first;
  }
  auto ctx = iter->second;
  if (!ctx->read_enable && readable) {
    ctx->socket.async_wait(asio::ip::tcp::socket::wait_read,
      [self, ctx, fd](asio::error_code ec){
      if (!ctx->read_enable) {
        return;
      }
      if (ec) {
        return;
      }
      ctx->read_enable = false;
      fd_t w = ARES_SOCKET_BAD;
      fd_t r = fd;
      ::ares_process_fd(self->channel_, r, w);
    });
  }
  if (!ctx->write_enable && writable) {
    ctx->socket.async_wait(asio::ip::tcp::socket::wait_write,
      [self, ctx, fd](asio::error_code ec){
      if (!ctx->write_enable) {
        return;
      }
      if (ec) {
        return;
      }
      ctx->write_enable = false;
      fd_t w = fd;
      fd_t r = ARES_SOCKET_BAD;
      ::ares_process_fd(self->channel_, r, w);
    });
  }
  ctx->read_enable |= readable;
  ctx->write_enable |= writable;
}

void CAresResolver::AsyncResolve(const std::string& host,
                                 const std::string& service,
                                 AsyncResolveCallback cb) {
  struct async_resolve_ctx {
    async_resolve_ctx(CAresResolver* s, AsyncResolveCallback c)
     : self(s), cb(c) {}
    scoped_refptr<CAresResolver> self;
    AsyncResolveCallback cb;
    std::string host;
    std::string service;
  };
  auto ctx = std::make_unique<async_resolve_ctx>(this, std::move(cb));
  ctx->host = host;
  ctx->service = service;
  struct ares_addrinfo_hints hints = {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = 0;
  hints.ai_protocol = 0;
  hints.ai_flags = ARES_AI_CANONNAME;
  ::ares_getaddrinfo(channel_, ctx->host.c_str(), ctx->service.c_str(), &hints,
    [](void *arg, int status, int timeouts, struct ares_addrinfo *result) {
      auto ctx = std::unique_ptr<async_resolve_ctx>(reinterpret_cast<async_resolve_ctx*>(arg));
      auto self = ctx->self;
      auto cb = ctx->cb;
      asio::error_code ec;
      self->resolve_timer_.cancel(ec);
      if (timeouts > 0) {
        cb(asio::error::timed_out, {});
        return;
      }
      if (status != ARES_SUCCESS) {
        cb(asio::error::not_found, {});
        return;
      }
      // Convert struct ares_addrinfo into normal struct addrinfo
      struct ares_addrinfo_node* next = result->nodes;
      struct addrinfo *addrinfo = new struct addrinfo;
      addrinfo->ai_next = nullptr;

      // Iterate the output
      struct addrinfo *next_addrinfo = addrinfo;
      while (next) {
        next_addrinfo->ai_next = new struct addrinfo;
        next_addrinfo = next_addrinfo->ai_next;

        next_addrinfo->ai_flags = next->ai_flags;
        next_addrinfo->ai_family = next->ai_family;
        next_addrinfo->ai_socktype = next->ai_socktype;
        next_addrinfo->ai_protocol = next->ai_protocol;
        next_addrinfo->ai_addrlen = next->ai_addrlen;
        next_addrinfo->ai_canonname = nullptr;
        next_addrinfo->ai_addr = next->ai_addr;
        next_addrinfo->ai_next = nullptr;

        next = next->ai_next;
      }
      struct addrinfo *prev_addrinfo = addrinfo;
      addrinfo = addrinfo->ai_next;
      delete prev_addrinfo;
      cb(asio::error_code(), asio::ip::tcp::resolver::results_type::create(addrinfo, ctx->host, ctx->service));
      while ((next_addrinfo = addrinfo->ai_next)) {
        delete addrinfo;
        addrinfo = next_addrinfo;
      }
  }, ctx.release());
  struct timeval tv {};
  struct timeval *tvp = ::ares_timeout(channel_, nullptr, &tv);
  if (tvp) {
    resolve_timer_.expires_from_now(std::chrono::seconds(tvp->tv_sec) +
                                    std::chrono::microseconds(tvp->tv_usec));
  }
  scoped_refptr<CAresResolver> self(this);
  resolve_timer_.async_wait([self](
    asio::error_code ec){
      if (ec == asio::error::operation_aborted) {
        return;
      }
      if (ec) {
        return;
      }
      fd_t w = ARES_SOCKET_BAD;
      fd_t r = ARES_SOCKET_BAD;
      ::ares_process_fd(self->channel_, r, w);
  });
}
