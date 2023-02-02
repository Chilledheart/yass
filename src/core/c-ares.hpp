// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_C_ARES_HPP
#define H_C_ARES_HPP
#include <ares.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "core/asio.hpp"
#include "core/scoped_refptr.hpp"
#include "core/ref_counted.hpp"

class CAresResolver : public RefCountedThreadSafe<CAresResolver> {
#ifdef _WIN32
  using fd_t = SOCKET;
#else
  using fd_t = int;
#endif
 public:
  CAresResolver(asio::io_context &io_context);
  static scoped_refptr<CAresResolver> Create(asio::io_context &io_context) {
    return MakeRefCounted<CAresResolver>(io_context);
  }
  ~CAresResolver();

  int Init(int timeout_ms, int retries);
  void Destroy();

  using AsyncResolveCallback = std::function<void(asio::error_code ec,
                                                  asio::ip::tcp::resolver::results_type)>;
  void AsyncResolve(const std::string& host, const std::string& service,
                    AsyncResolveCallback cb);

  void Cancel();

 private:
  struct ResolverPerContext : public RefCountedThreadSafe<ResolverPerContext> {
    static scoped_refptr<ResolverPerContext> Create(asio::io_context &io_context, fd_t fd) {
      return MakeRefCounted<ResolverPerContext>(io_context, fd);
    }
    ResolverPerContext(asio::io_context &io_context, fd_t fd)
      : socket(io_context, asio::ip::udp::v4(), fd) {}
    ~ResolverPerContext() {
      asio::error_code ec;
      static_cast<void>(socket.release(ec));
    }

    asio::ip::udp::socket socket;
    bool read_enable = false;
    bool write_enable = false;
  };

  static void OnSockState(void *arg, fd_t fd, int readable, int writable);
  void OnSockStateReadable(scoped_refptr<ResolverPerContext> ctx, fd_t fd);
  void OnSockStateWritable(scoped_refptr<ResolverPerContext> ctx, fd_t fd);

  void OnAsyncWait();

  asio::io_context &io_context_;

  ares_channel channel_;
  ares_options ares_opts_;
  std::unordered_map<fd_t, scoped_refptr<ResolverPerContext>> fd_map_;

  char lookups_[3] = "fb";
  int timeout_ms_;
  asio::steady_timer resolve_timer_;
  bool canceled_ = false;
};
#endif // H_C_ARES_HPP
