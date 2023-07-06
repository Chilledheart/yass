// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifdef HAVE_C_ARES

#include "core/c-ares.hpp"

#define CURL_TIMEOUT_RESOLVE 300 /* when using asynch methods, we allow this
                                    many seconds for a name resolve */

namespace {

asio::error_code AresToAsioError(int status) {
  asio::error_code ec;
  switch(status) {
    case ARES_SUCCESS:
      break;
    case ARES_ENODATA:
    case ARES_EFORMERR:
    case ARES_ESERVFAIL:
    case ARES_ENOTFOUND:
    case ARES_ENOTIMP:
    case ARES_EBADRESP:
    case ARES_ENONAME:
      ec = asio::error::host_not_found;
      break;
    case ARES_EREFUSED:
    case ARES_ECONNREFUSED:
      ec = asio::error::connection_refused;
      break;
    case ARES_ETIMEOUT:
      ec = asio::error::timed_out;
      break;
    case ARES_EOF:
      ec = asio::error::eof;
      break;
    case ARES_EFILE:
      ec = asio::error::bad_descriptor;
      break;
    case ARES_ENOMEM:
      ec = asio::error::no_memory;
      break;
    case ARES_ECANCELLED:
    case ARES_EDESTRUCTION:
      ec = asio::error::operation_aborted;
      break;
    case ARES_EBADQUERY:
    case ARES_EBADNAME:
    case ARES_EBADFAMILY:
    case ARES_EBADSTR:
    case ARES_EBADHINTS:
    default:
      ec = asio::error::invalid_argument;
      break;
  }
  return ec;
}
} // anonymous namespace

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
  VLOG(1) << "c-ares resolver freed memory";
}

int CAresResolver::Init(int timeout_ms) {
  ares_opts_.lookups = lookups_;
  ares_opts_.sock_state_cb_data = this;
  ares_opts_.sock_state_cb = &CAresResolver::OnSockState;
  int ret = ::ares_init_options(&channel_, &ares_opts_,
                                ARES_OPT_LOOKUPS | ARES_OPT_SOCK_STATE_CB);
  if (ret) {
    LOG(WARNING) << "ares_init_options failure: " << ares_strerror(ret);
  } else {
    init_ = true;
  }
  timeout_ms_ = timeout_ms ? timeout_ms : CURL_TIMEOUT_RESOLVE * 1000;
  return ret;
}

void CAresResolver::Destroy() {
  if (!init_) {
    return;
  }
  Cancel();
  ::ares_destroy(channel_);
}

void CAresResolver::OnSockState(void *arg, fd_t fd, int readable, int writable) {
  // Might be involved by Destory in dtor.
  // Cannot call AddRef directly.
  auto resolver = reinterpret_cast<CAresResolver*>(arg);
  auto self = scoped_refptr(resolver);
  auto iter = self->fd_map_.find(fd);
  if (!readable && !writable) {
    if (iter != self->fd_map_.end()) {
      auto ctx = iter->second;
      self->fd_map_.erase(iter);
      ctx->read_enable = false;
      ctx->write_enable = false;
    }
    return;
  }
  if (iter == self->fd_map_.end()) {
#ifdef _WIN32
    WSAPROTOCOL_INFOW pi {};
    if (::WSADuplicateSocketW(fd, ::GetCurrentProcessId(), &pi) != 0) {
      PLOG(WARNING) << "c-ares: WSASocket failed to dup";
      resolver->Cancel();
      return;
    }
    fd_t fd2 = ::WSASocketW(pi.iAddressFamily, pi.iSocketType, pi.iProtocol, &pi, 0, 0);
    if (fd2 == INVALID_SOCKET) {
      PLOG(WARNING) << "c-ares: WSASocket failed to dup";
      resolver->Cancel();
      return;
    }
#else
    fd_t fd2 = dup(fd);
    if (fd2 < 0) {
      PLOG(WARNING) << "c-ares: file descriptor failed to dup";
      resolver->Cancel();
      return;
    }
#endif
    auto value = std::make_pair(fd, ResolverPerContext::Create(self->io_context_, fd2));
    iter = self->fd_map_.insert(value).first;
  }
  auto ctx = iter->second;
  if (!ctx->read_enable && readable) {
    self->OnSockStateReadable(ctx, fd);
  }
  if (!ctx->write_enable && writable) {
    self->OnSockStateWritable(ctx, fd);
  }
  ctx->read_enable = readable;
  ctx->write_enable = writable;
}

void CAresResolver::OnSockStateReadable(scoped_refptr<ResolverPerContext> ctx, fd_t fd) {
  auto self = scoped_refptr(this);
  ctx->socket.async_wait(asio::ip::tcp::socket::wait_read,
    [this, self, ctx, fd](asio::error_code ec) {
    if (!ctx->read_enable) {
      return;
    }
    ctx->read_enable = false;
    if (ec) {
      return;
    }
    fd_t r = fd;
    fd_t w = ARES_SOCKET_BAD;
    ::ares_process_fd(channel_, r, w);
  });
}

void CAresResolver::OnSockStateWritable(scoped_refptr<ResolverPerContext> ctx, fd_t fd) {
  auto self = scoped_refptr(this);
  ctx->socket.async_wait(asio::ip::tcp::socket::wait_write,
    [this, self, ctx, fd](asio::error_code ec) {
    if (!ctx->write_enable) {
      return;
    }
    ctx->write_enable = false;
    if (ec) {
      return;
    }
    fd_t r = ARES_SOCKET_BAD;
    fd_t w = fd;
    ::ares_process_fd(channel_, r, w);
  });
}

void CAresResolver::AsyncResolve(const std::string& host,
                                 const std::string& service,
                                 AsyncResolveCallback cb) {
  DCHECK(init_) << "Init should be called before use";
  DCHECK(done_) << "Another resolve is in progress";

  done_ = false;
  expired_ = false;

  struct async_resolve_ctx {
    async_resolve_ctx(scoped_refptr<CAresResolver> s, AsyncResolveCallback c)
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
  /* Since the service is a numerical one, set the hint flags
   * accordingly to save a call to getservbyname in inside C-Ares
   */
  hints.ai_flags = ARES_AI_CANONNAME | ARES_AI_NUMERICSERV;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  ::ares_getaddrinfo(channel_, host.c_str(), service.c_str(), &hints,
    [](void *arg, int status, int timeouts, struct ares_addrinfo *result) {
      auto ctx = std::unique_ptr<async_resolve_ctx>(reinterpret_cast<async_resolve_ctx*>(arg));
      auto self = ctx->self;
      self->done_ = true;
      if (status != ARES_SUCCESS && self->expired_) {
        status = ARES_ETIMEOUT;
      }
      if (status == ARES_ECANCELLED || status == ARES_EDESTRUCTION) {
        return;
      }
      auto cb = ctx->cb;
      self->resolve_timer_.cancel();
      (void)timeouts;
      if (status != ARES_SUCCESS) {
        asio::error_code ec = AresToAsioError(status);
        cb(ec, {});
        return;
      }
      // Convert struct ares_addrinfo into normal struct addrinfo
      struct ares_addrinfo_node* next = result->nodes;
      struct addrinfo *addrinfo = new struct addrinfo;
      addrinfo->ai_next = nullptr;
      // If hints.ai_flags includes the AI_CANONNAME flag, then the
      // ai_canonname field of the first of the addrinfo structures in the
      // returned list is set to point to the official name of the host.
      char* canon_name = result->cnames ? result->cnames->name : nullptr;

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
        next_addrinfo->ai_canonname = canon_name;
        next_addrinfo->ai_addr = next->ai_addr;
        next_addrinfo->ai_next = nullptr;

        canon_name = nullptr;

        next = next->ai_next;
      }
      struct addrinfo *prev_addrinfo = addrinfo;
      addrinfo = addrinfo->ai_next;
      delete prev_addrinfo;
      cb(asio::error_code(), asio::ip::tcp::resolver::results_type::create(addrinfo, ctx->host, ctx->service));
      // free both of addrinfo and ares_addrinfo structures
      while (addrinfo) {
        next_addrinfo = addrinfo->ai_next;
        delete addrinfo;
        addrinfo = next_addrinfo;
      }
      ::ares_freeaddrinfo(result);
  }, ctx.release());
  if (!done_) {
    OnAsyncWait();
  }
}

void CAresResolver::Cancel() {
  DCHECK(init_);
  if (done_) {
    return;
  }
  resolve_timer_.cancel();
  ::ares_cancel(channel_);
}

void CAresResolver::OnAsyncWait() {
  scoped_refptr<CAresResolver> self(this);
  struct timeval tv, maxtime;
  maxtime.tv_sec = timeout_ms_ / 1000;
  maxtime.tv_usec = (timeout_ms_ % 1000) * 1000;
  struct timeval *tvp = ::ares_timeout(channel_, &maxtime, &tv);
  DCHECK(tvp);

  resolve_timer_.expires_after(std::chrono::seconds(tvp->tv_sec) +
                               std::chrono::microseconds(tvp->tv_usec + 10));
  resolve_timer_.async_wait([this, self](
    asio::error_code ec) {
      if (ec == asio::error::operation_aborted) {
        return;
      }
      if (ec) {
        return;
      }
      if (done_) {
        return;
      }
      expired_ = true;
      Cancel();
      fd_t r = ARES_SOCKET_BAD;
      fd_t w = ARES_SOCKET_BAD;
      ::ares_process_fd(channel_, r, w);
  });
}

#endif // HAVE_C_ARES
