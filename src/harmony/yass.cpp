// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#ifdef __OHOS__

#include "harmony/yass.hpp"

#include <base/posix/eintr_wrapper.h>
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <napi/native_api.h>

#include <unistd.h>
#include <memory>
#include <thread>

#include "cli/cli_connection_stats.hpp"
#include "cli/cli_worker.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "harmony/tun2proxy.h"
#include "version.h"

typedef enum {
  HILOG_LOG_DEBUG = 3,
  HILOG_LOG_INFO = 4,
  HILOG_LOG_WARN = 5,
  HILOG_LOG_ERROR = 6,
  HILOG_LOG_FATAL = 7,
} HILOG_LogLevel;

extern "C" bool OH_LOG_IsLoggable(unsigned int domain, const char* tag, HILOG_LogLevel level);

using namespace std::string_literals;

static constexpr const char kLogTag[] = YASS_APP_NAME;
static constexpr const unsigned int kLogDomain = 0x0;

static napi_threadsafe_function setProtectFdCallbackFunc = nullptr;

struct AsyncProtectFdEx_t {
  int fd;
  int write_end;
};

static constexpr const char kAsyncResourceName[] = "Thread-safe SetProtectFd";

static void setProtectFdWriteResult(int fd, napi_status status) {
  int result = status;
  if (HANDLE_EINTR(write(fd, &result, sizeof(result))) < 0) {
    PLOG(WARNING) << "write failed to pipe";
  }
}

int setProtectFd(int fd) {
  int pipefd[2] = {-1, -1};
  if (pipe(pipefd) == -1) {
    PLOG(WARNING) << "create pipe failed";
    return -1;
  };
  int read_end = pipefd[0];

  auto ctx = std::make_unique<AsyncProtectFdEx_t>();
  ctx->fd = fd;
  ctx->write_end = pipefd[1];

  auto status = napi_acquire_threadsafe_function(setProtectFdCallbackFunc);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_acquire_threadsafe_function: " << status;
    return -1;
  }

  auto ctx_raw = ctx.release();
  status = napi_call_threadsafe_function(setProtectFdCallbackFunc, ctx_raw, napi_tsfn_blocking);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_call_threadsafe_function: " << status;
    delete ctx_raw;
    return -1;
  }

  status = napi_release_threadsafe_function(setProtectFdCallbackFunc, napi_tsfn_release);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_release_threadsafe_function: " << status;
    return -1;
  }

  // wait on async work done
  int result;
  if (HANDLE_EINTR(read(read_end, &result, sizeof(result))) < 0) {
    PLOG(WARNING) << "read failed from pipe";
    IGNORE_EINTR(close(pipefd[0]));
    IGNORE_EINTR(close(pipefd[1]));
    return -1;
  }
  status = static_cast<napi_status>(result);
  IGNORE_EINTR(close(pipefd[0]));
  IGNORE_EINTR(close(pipefd[1]));
  LOG(WARNING) << "setProtectFd: status: " << status;
  return 0;
}

static napi_value setProtectFdCallingJSCallback(napi_env env, napi_callback_info info) {
  void* data;
  auto status = napi_get_cb_info(env, info, nullptr, nullptr, nullptr, &data);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_get_cb_info failed");
    return nullptr;
  }

  setProtectFdWriteResult((uintptr_t)data, napi_ok);
  return nullptr;
}

static void setProtectFdCallingJS(napi_env env, napi_value /*js_cb*/, void* context, void* data) {
  std::unique_ptr<AsyncProtectFdEx_t> ctx(reinterpret_cast<AsyncProtectFdEx_t*>(data));
  int fd_value = ctx->fd;
  int write_end = ctx->write_end;

  auto cb_ref = reinterpret_cast<napi_ref>(context);

  napi_value cb;
  auto status = napi_get_reference_value(env, cb_ref, &cb);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_reference_value: " << status;
    setProtectFdWriteResult(fd_value, status);
    return;
  }

  napi_valuetype type;
  status = napi_typeof(env, cb, &type);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_typeof failed: " << status;
    setProtectFdWriteResult(fd_value, status);
    return;
  }

  if (type != napi_function) {
    LOG(WARNING) << "napi_typeof unexpected: " << type;
    setProtectFdWriteResult(fd_value, status);
    return;
  }

  if (env == nullptr) {
    LOG(WARNING) << "null env";
    setProtectFdWriteResult(fd_value, status);
    return;
  }

  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_global: " << status;
    setProtectFdWriteResult(fd_value, status);
    return;
  }

  napi_value fd;
  napi_value callback;
  status = napi_create_int32(env, fd_value, &fd);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_create_int32: " << status;
    setProtectFdWriteResult(fd_value, status);
    return;
  }

  status = napi_create_function(env, nullptr, 0, setProtectFdCallingJSCallback, (void*)(uintptr_t)write_end, &callback);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_create_function: " << status;
    setProtectFdWriteResult(fd_value, status);
    return;
  }

  napi_value argv[] = {fd, callback};
  status = napi_call_function(env, global, cb, std::size(argv), argv, nullptr);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_call_function: " << status;
    setProtectFdWriteResult(fd_value, status);
    return;
  }

  status = napi_delete_reference(env, cb_ref);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_delete_reference: " << status;
    setProtectFdWriteResult(fd_value, status);
    return;
  }

  setProtectFdWriteResult(fd_value, status);
}

static napi_value setProtectFdCallback(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 1;
  napi_value args[1] = {nullptr};
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok || argc != 1) {
    napi_throw_error(env, nullptr, "napi_get_cb_info failed");
    return nullptr;
  }

  napi_value cb = args[0];
  napi_valuetype type;
  status = napi_typeof(env, cb, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_function) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_function");
    return nullptr;
  }

  napi_ref cb_ref;
  status = napi_create_reference(env, cb, 1, &cb_ref);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_reference failed");
    return nullptr;
  }

  napi_value work_name;
  // Specify a name to describe this asynchronous operation.
  status = napi_create_string_utf8(env, kAsyncResourceName, sizeof(kAsyncResourceName) - 1, &work_name);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  // Create a thread-safe N-API callback function correspond to the C/C++ callback function
  status = napi_create_threadsafe_function(
      env, cb, nullptr, work_name, 0, 1, nullptr, nullptr, cb_ref,
      setProtectFdCallingJS,     // the C/C++ callback function
      &setProtectFdCallbackFunc  // out: the asynchronous thread-safe JavaScript function
  );
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_threadsafe_function failed");
    return nullptr;
  }

  return nullptr;
}

static napi_value setProtectFdCallbackCleanup(napi_env env, napi_callback_info info) {
  if (setProtectFdCallbackFunc == nullptr) {
    return nullptr;
  }
  // Clean up the thread-safe function and the work item associated with this
  auto status = napi_release_threadsafe_function(setProtectFdCallbackFunc, napi_tsfn_release);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_release_threadsafe_function failed");
    return nullptr;
  }
  return nullptr;
}

static std::unique_ptr<std::thread> g_tun2proxy_thread;

// 4 arguments:
//
// proxy_url
// tun_fd
// tun_mtu
// dns_over_tcp
static napi_value initTun2proxy(napi_env env, napi_callback_info info) {
  napi_value args[4]{};
  size_t argc = std::size(args);
  auto status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok || argc != std::size(args)) {
    napi_throw_error(env, nullptr, "napi_get_cb_info failed");
    return nullptr;
  }

  napi_value proxy_url = args[0];
  napi_value tun_fd = args[1];
  napi_value tun_mtu = args[2];
  napi_value dns_over_tcp = args[3];

  napi_valuetype type;
  status = napi_typeof(env, proxy_url, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_string) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_string");
    return nullptr;
  }

  char proxy_url_buf[256];
  size_t result;
  status = napi_get_value_string_utf8(env, proxy_url, proxy_url_buf, sizeof(proxy_url_buf), &result);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_get_value_string_utf8 failed");
    return nullptr;
  }

  status = napi_typeof(env, tun_fd, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_number) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_number");
    return nullptr;
  }

  int64_t tun_fd_int;
  status = napi_get_value_int64(env, tun_fd, &tun_fd_int);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_get_value_int64 failed");
    return nullptr;
  }

  status = napi_typeof(env, tun_mtu, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_number) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_number");
    return nullptr;
  }

  int64_t tun_mtu_int;
  status = napi_get_value_int64(env, tun_mtu, &tun_mtu_int);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_get_value_int64 failed");
    return nullptr;
  }

  int log_level = 0;  // stands for rust log level, 0 stands for no log output

  if (OH_LOG_IsLoggable(kLogDomain, kLogTag, HILOG_LOG_DEBUG)) {
    log_level = 5;  // TRACE
  } else if (OH_LOG_IsLoggable(kLogDomain, kLogTag, HILOG_LOG_DEBUG)) {
    log_level = 4;  // DEBUG
  } else if (OH_LOG_IsLoggable(kLogDomain, kLogTag, HILOG_LOG_INFO)) {
    log_level = 3;  // INFO
  } else if (OH_LOG_IsLoggable(kLogDomain, kLogTag, HILOG_LOG_WARN)) {
    log_level = 2;  // WARN
  } else if (OH_LOG_IsLoggable(kLogDomain, kLogTag, HILOG_LOG_ERROR)) {
    log_level = 1;  // ERROR
  }

  status = napi_typeof(env, dns_over_tcp, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_boolean) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_boolean");
    return nullptr;
  }

  bool dns_over_tcp_int;
  status = napi_get_value_bool(env, dns_over_tcp, &dns_over_tcp_int);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_get_value_bool failed");
    return nullptr;
  }

  void* ptr = tun2proxy_init(proxy_url_buf, tun_fd_int, tun_mtu_int, log_level, dns_over_tcp_int);
  if (ptr == nullptr) {
    LOG(WARNING) << "tun2proxy_init failed";
  }

  napi_value value;
  status = napi_create_int64(env, reinterpret_cast<int64_t>(ptr), &value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_int64 failed");
    return nullptr;
  }

  return value;
}

static napi_value runTun2proxy(napi_env env, napi_callback_info info) {
  napi_value args[1]{};
  size_t argc = std::size(args);
  auto status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok || argc != std::size(args)) {
    napi_throw_error(env, nullptr, "napi_get_cb_info failed");
    return nullptr;
  }

  napi_value ptr = args[0];

  napi_valuetype type;
  status = napi_typeof(env, ptr, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_number) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_number");
    return nullptr;
  }

  int64_t ptr_int;
  status = napi_get_value_int64(env, ptr, &ptr_int);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_get_value_int64 failed");
    return nullptr;
  }

  g_tun2proxy_thread = std::make_unique<std::thread>([=] {
    if (!SetCurrentThreadName("tun2proxy")) {
      PLOG(WARNING) << "failed to set thread name";
    }
    if (!SetCurrentThreadPriority(ThreadPriority::ABOVE_NORMAL)) {
      PLOG(WARNING) << "failed to set thread priority";
    }
    int ret = tun2proxy_run(reinterpret_cast<void*>(ptr_int));
    if (ret != 0) {
      LOG(WARNING) << "tun2proxy_run failed: " << ret;
    }
  });
  return nullptr;
}

static napi_value stopTun2proxy(napi_env env, napi_callback_info info) {
  napi_value args[1]{};
  size_t argc = std::size(args);
  auto status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok || argc != std::size(args)) {
    napi_throw_error(env, nullptr, "napi_get_cb_info failed");
    return nullptr;
  }

  napi_value ptr = args[0];

  napi_valuetype type;
  status = napi_typeof(env, ptr, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_number) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_number");
    return nullptr;
  }

  int64_t ptr_int;
  status = napi_get_value_int64(env, ptr, &ptr_int);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_get_value_int64 failed");
    return nullptr;
  }

  if (!g_tun2proxy_thread) {
    return nullptr;
  }

  int ret = tun2proxy_shutdown(reinterpret_cast<void*>(ptr_int));
  if (ret != 0) {
    LOG(WARNING) << "tun2proxy_shutdown failed: " << ret;
  }
  g_tun2proxy_thread->join();
  g_tun2proxy_thread.reset();

  tun2proxy_destroy(reinterpret_cast<void*>(ptr_int));
  return nullptr;
}

static constexpr const char kAsyncStartWorkerResourceName[] = "Thread-safe StartWorker";
struct AsyncStartCtx {
  asio::error_code ec;
  int port_num;
};

static void startWorkerCallingJS(napi_env env, napi_value /*js_cb*/, void* context, void* data) {
  napi_ref cb_ref = reinterpret_cast<napi_ref>(context);

  std::unique_ptr<AsyncStartCtx> ctx(reinterpret_cast<AsyncStartCtx*>(data));
  std::ostringstream ss;
  if (ctx->ec) {
    ss << ctx->ec;
  }
  std::string ec_str = ss.str();
  int port_num = ctx->port_num;

  if (env == nullptr) {
    LOG(WARNING) << "null env";
    return;
  }

  napi_value global;
  auto status = napi_get_global(env, &global);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_global: " << status;
    return;
  }

  napi_value cb;
  status = napi_get_reference_value(env, cb_ref, &cb);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_reference_value: " << status;
    return;
  }

  napi_valuetype type;
  status = napi_typeof(env, cb, &type);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_typeof failed: " << status;
    return;
  }

  if (type != napi_function) {
    LOG(WARNING) << "napi_typeof unexpected: " << type;
    return;
  }

  napi_value err_msg;
  status = napi_create_string_utf8(env, ec_str.c_str(), ec_str.size(), &err_msg);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_create_string_utf8: " << status;
    return;
  }

  napi_value port;
  status = napi_create_int32(env, port_num, &port);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_create_int32: " << status;
    return;
  }

  napi_value result;
  napi_value argv[] = {err_msg, port};
  status = napi_call_function(env, global, cb, std::size(argv), argv, &result);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_call_function: " << status;
    return;
  }

  status = napi_delete_reference(env, cb_ref);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_delete_reference: " << status;
    return;
  }
}

static std::unique_ptr<Worker> g_worker;

static napi_value startWorker(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value args[1]{};
  size_t argc = std::size(args);
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok || argc != std::size(args)) {
    napi_throw_error(env, nullptr, "napi_get_cb_info failed");
    return nullptr;
  }

  napi_value cb = args[0];
  napi_valuetype type;
  status = napi_typeof(env, cb, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_function) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_function");
    return nullptr;
  }

  napi_ref cb_ref;
  status = napi_create_reference(env, cb, 1, &cb_ref);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_reference failed");
    return nullptr;
  }

  napi_value work_name;
  // Specify a name to describe this asynchronous operation.
  status = napi_create_string_utf8(env, kAsyncStartWorkerResourceName, sizeof(kAsyncStartWorkerResourceName) - 1,
                                   &work_name);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  napi_threadsafe_function startWorkerCallbackFunc;

  // Create a thread-safe N-API callback function correspond to the C/C++ callback function
  status = napi_create_threadsafe_function(
      env, cb, nullptr, work_name, 0, 1, nullptr, nullptr, cb_ref, startWorkerCallingJS,  // the C/C++ callback function
      &startWorkerCallbackFunc  // out: the asynchronous thread-safe JavaScript function
  );
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_threadsafe_function failed");
    return nullptr;
  }

  g_worker->Start([startWorkerCallbackFunc](asio::error_code ec) {
    if (!ec) {
      config::SaveConfig();
    }
    std::unique_ptr<AsyncStartCtx> ctx = std::make_unique<AsyncStartCtx>();
    ctx->ec = ec;
    ctx->port_num = ec ? 0 : g_worker->GetLocalPort();

    auto status = napi_acquire_threadsafe_function(startWorkerCallbackFunc);
    if (status != napi_ok) {
      LOG(WARNING) << "napi_acquire_threadsafe_function: " << status;
    }

    auto ctx_raw = ctx.release();
    status = napi_call_threadsafe_function(startWorkerCallbackFunc, ctx_raw, napi_tsfn_blocking);
    if (status != napi_ok) {
      LOG(WARNING) << "napi_call_threadsafe_function: " << status;
      delete ctx_raw;
    }

    status = napi_release_threadsafe_function(startWorkerCallbackFunc, napi_tsfn_release);
    if (status != napi_ok) {
      LOG(WARNING) << "napi_release_threadsafe_function: " << status;
    }

    // release callback
    status = napi_release_threadsafe_function(startWorkerCallbackFunc, napi_tsfn_release);
    if (status != napi_ok) {
      LOG(WARNING) << "napi_release_threadsafe_function: " << status;
    }
  });
  return nullptr;
}

static constexpr const char kAsyncStopWorkerResourceName[] = "Thread-safe StopWorker";

static void stopWorkerCallingJS(napi_env env, napi_value /*js_cb*/, void* context, void* data) {
  napi_ref cb_ref = reinterpret_cast<napi_ref>(context);

  if (env == nullptr) {
    LOG(WARNING) << "null env";
    return;
  }

  napi_value global;
  auto status = napi_get_global(env, &global);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_global: " << status;
    return;
  }

  napi_value cb;
  status = napi_get_reference_value(env, cb_ref, &cb);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_reference_value: " << status;
    return;
  }

  napi_valuetype type;
  status = napi_typeof(env, cb, &type);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_typeof failed: " << status;
    return;
  }

  if (type != napi_function) {
    LOG(WARNING) << "napi_typeof unexpected: " << type;
    return;
  }

  napi_value result;
  status = napi_call_function(env, global, cb, 0, nullptr, &result);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_call_function: " << status;
    return;
  }

  status = napi_delete_reference(env, cb_ref);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_delete_reference: " << status;
    return;
  }
}

static napi_value stopWorker(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value args[1]{};
  size_t argc = std::size(args);
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok || argc != std::size(args)) {
    napi_throw_error(env, nullptr, "napi_get_cb_info failed");
    return nullptr;
  }

  napi_value cb = args[0];
  napi_valuetype type;
  status = napi_typeof(env, cb, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_function) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_function");
    return nullptr;
  }

  napi_ref cb_ref;
  status = napi_create_reference(env, cb, 1, &cb_ref);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_reference failed");
    return nullptr;
  }

  napi_value work_name;
  // Specify a name to describe this asynchronous operation.
  status =
      napi_create_string_utf8(env, kAsyncStopWorkerResourceName, sizeof(kAsyncStopWorkerResourceName) - 1, &work_name);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  napi_threadsafe_function stopWorkerCallbackFunc;

  // Create a thread-safe N-API callback function correspond to the C/C++ callback function
  status = napi_create_threadsafe_function(
      env, cb, nullptr, work_name, 0, 1, nullptr, nullptr, cb_ref, stopWorkerCallingJS,  // the C/C++ callback function
      &stopWorkerCallbackFunc  // out: the asynchronous thread-safe JavaScript function
  );
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_threadsafe_function failed");
    return nullptr;
  }

  g_worker->Stop([stopWorkerCallbackFunc]() {
    auto status = napi_acquire_threadsafe_function(stopWorkerCallbackFunc);
    if (status != napi_ok) {
      LOG(WARNING) << "napi_acquire_threadsafe_function: " << status;
    }

    status = napi_call_threadsafe_function(stopWorkerCallbackFunc, nullptr, napi_tsfn_blocking);
    if (status != napi_ok) {
      LOG(WARNING) << "napi_call_threadsafe_function: " << status;
    }

    status = napi_release_threadsafe_function(stopWorkerCallbackFunc, napi_tsfn_release);
    if (status != napi_ok) {
      LOG(WARNING) << "napi_release_threadsafe_function: " << status;
    }

    // release callback
    status = napi_release_threadsafe_function(stopWorkerCallbackFunc, napi_tsfn_release);
    if (status != napi_ok) {
      LOG(WARNING) << "napi_release_threadsafe_function: " << status;
    }
  });
  return nullptr;
}

static uint64_t g_last_sync_time;
static uint64_t g_last_tx_bytes;
static uint64_t g_last_rx_bytes;

// return { tx_rate, rx_rate }
static napi_value getTransferRate(napi_env env, napi_callback_info info) {
  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - g_last_sync_time;
  static uint64_t rx_rate = 0;
  static uint64_t tx_rate = 0;
  if (delta_time > NS_PER_SECOND) {
    uint64_t rx_bytes = cli::total_rx_bytes;
    uint64_t tx_bytes = cli::total_tx_bytes;
    rx_rate = static_cast<double>(rx_bytes - g_last_rx_bytes) / delta_time * NS_PER_SECOND;
    tx_rate = static_cast<double>(tx_bytes - g_last_tx_bytes) / delta_time * NS_PER_SECOND;
    g_last_sync_time = sync_time;
    g_last_rx_bytes = rx_bytes;
    g_last_tx_bytes = tx_bytes;
  }

  std::stringstream rx_ss, tx_ss;
  HumanReadableByteCountBin(&rx_ss, rx_rate);
  rx_ss << "/s";

  std::string rx_ss_str = rx_ss.str();
  napi_value rx_rate_value;
  auto status = napi_create_string_utf8(env, rx_ss_str.c_str(), rx_ss_str.size(), &rx_rate_value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  HumanReadableByteCountBin(&tx_ss, tx_rate);
  tx_ss << "/s";

  std::string tx_ss_str = tx_ss.str();
  napi_value tx_rate_value;
  status = napi_create_string_utf8(env, tx_ss_str.c_str(), tx_ss_str.size(), &tx_rate_value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  napi_value results;
  status = napi_create_array_with_length(env, 2, &results);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_array_with_length failed");
    return nullptr;
  }

  status = napi_set_element(env, results, 0, rx_rate_value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_set_element failed");
    return nullptr;
  }

  status = napi_set_element(env, results, 1, tx_rate_value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_set_element failed");
    return nullptr;
  }

  std::stringstream ss;
  ss << "Connected connections";
  ss << " tx rate: " << rx_rate;
  ss << " rx rate: " << tx_rate;

  VLOG(1) << ss.str();

  return results;
}

// Passing argument in a array
// server_host
// server_sni
// server_port
// username
// password
// method
// doh_url
// dot_host
// timeout
static napi_value saveConfig(napi_env env, napi_callback_info info) {
  napi_value args[9]{};
  size_t argc = std::size(args);
  auto status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok || argc != std::size(args)) {
    napi_throw_error(env, nullptr, "napi_get_cb_info failed");
    return nullptr;
  }

  napi_valuetype type;
  std::vector<std::string> argList;

  for (napi_value arg : args) {
    status = napi_typeof(env, arg, &type);
    if (status != napi_ok) {
      napi_throw_error(env, nullptr, "napi_typeof failed");
      return nullptr;
    }

    if (type != napi_string) {
      napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_string");
      return nullptr;
    }
    char buf[256];
    size_t result;
    status = napi_get_value_string_utf8(env, arg, buf, sizeof(buf), &result);
    if (status != napi_ok) {
      napi_throw_error(env, nullptr, "napi_get_value_string_utf8 failed");
      return nullptr;
    }
    argList.push_back(std::string(buf, result));
  }

  std::string server_host = argList[0];
  std::string server_sni = argList[1];
  std::string server_port = argList[2];
  std::string username = argList[3];
  std::string password = argList[4];
  std::string method = argList[5];
  std::string doh_url = argList[6];
  std::string dot_host = argList[7];
  std::string timeout = argList[8];

  std::string local_host = "0.0.0.0"s;
  std::string local_port = "0"s;

  std::string err_msg = config::ReadConfigFromArgument(server_host, server_sni, server_port, username, password, method,
                                                       local_host, local_port, doh_url, dot_host, timeout);

  napi_value result;
  status = napi_create_string_utf8(env, err_msg.c_str(), err_msg.size(), &result);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }
  return result;
}

static napi_value getServerHost(napi_env env, napi_callback_info info) {
  napi_value value;
  std::string str = absl::GetFlag(FLAGS_server_host);
  auto status = napi_create_string_utf8(env, str.c_str(), str.size(), &value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  return value;
}

static napi_value getServerSNI(napi_env env, napi_callback_info info) {
  napi_value value;
  std::string str = absl::GetFlag(FLAGS_server_sni);
  auto status = napi_create_string_utf8(env, str.c_str(), str.size(), &value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  return value;
}

static napi_value getServerPort(napi_env env, napi_callback_info info) {
  napi_value value;
  auto status = napi_create_int32(env, absl::GetFlag(FLAGS_server_port), &value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_int32 failed");
    return nullptr;
  }

  return value;
}

static napi_value getUsername(napi_env env, napi_callback_info info) {
  napi_value value;
  std::string str = absl::GetFlag(FLAGS_username);
  auto status = napi_create_string_utf8(env, str.c_str(), str.size(), &value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  return value;
}

static napi_value getPassword(napi_env env, napi_callback_info info) {
  napi_value value;
  std::string str = absl::GetFlag(FLAGS_password);
  auto status = napi_create_string_utf8(env, str.c_str(), str.size(), &value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  return value;
}

static napi_value getCipher(napi_env env, napi_callback_info info) {
  napi_value value;
  auto status =
      napi_create_string_utf8(env, to_cipher_method_str(absl::GetFlag(FLAGS_method).method), NAPI_AUTO_LENGTH, &value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  return value;
}

static napi_value getCipherStrings(napi_env env, napi_callback_info info) {
  static constexpr const char* const method_names[] = {
#define XX(num, name, string) string,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };

  napi_value results;
  auto status = napi_create_array_with_length(env, std::size(method_names), &results);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_array_with_length failed");
    return nullptr;
  }

  for (size_t i = 0; i < std::size(method_names); ++i) {
    napi_value value;
    auto status = napi_create_string_utf8(env, method_names[i], NAPI_AUTO_LENGTH, &value);
    if (status != napi_ok) {
      napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
      return nullptr;
    }

    status = napi_set_element(env, results, i, value);
    if (status != napi_ok) {
      napi_throw_error(env, nullptr, "napi_set_element failed");
      return nullptr;
    }
  }

  return results;
}

static napi_value getDoHUrl(napi_env env, napi_callback_info info) {
  napi_value value;
  std::string str = absl::GetFlag(FLAGS_doh_url);
  auto status = napi_create_string_utf8(env, str.c_str(), str.size(), &value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  return value;
}

static napi_value getDoTHost(napi_env env, napi_callback_info info) {
  napi_value value;
  std::string str = absl::GetFlag(FLAGS_dot_host);
  auto status = napi_create_string_utf8(env, str.c_str(), str.size(), &value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  return value;
}

static napi_value getTimeout(napi_env env, napi_callback_info info) {
  napi_value value;
  auto status = napi_create_int32(env, absl::GetFlag(FLAGS_connect_timeout), &value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_int32 failed");
    return nullptr;
  }

  return value;
}

static napi_value initRoutine(napi_env env, napi_callback_info info) {
  napi_value args[2]{};
  size_t argc = std::size(args);
  auto status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok || argc != std::size(args)) {
    napi_throw_error(env, nullptr, "napi_get_cb_info failed");
    return nullptr;
  }

  napi_value cache_path = args[0];
  napi_value data_path = args[1];

  napi_valuetype type;
  status = napi_typeof(env, cache_path, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_string) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_string");
    return nullptr;
  }

  status = napi_typeof(env, data_path, &type);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (type != napi_string) {
    napi_throw_error(env, nullptr, "mismatched argument type, expected: napi_string");
    return nullptr;
  }

  char cache_path_buf[PATH_MAX + 1];
  size_t cache_path_size;
  status = napi_get_value_string_utf8(env, cache_path, cache_path_buf, sizeof(cache_path_buf), &cache_path_size);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_get_value_string_utf8 failed");
    return nullptr;
  }

  char data_path_buf[PATH_MAX + 1];
  size_t data_path_size;
  status = napi_get_value_string_utf8(env, data_path, data_path_buf, sizeof(data_path_buf), &data_path_size);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_get_value_string_utf8 failed");
    return nullptr;
  }

  std::string exe_path;
  GetExecutablePath(&exe_path);
  SetExecutablePath(exe_path);

  h_cache_dir = std::string(cache_path_buf, cache_path_size);
  h_data_dir = std::string(data_path_buf, data_path_size);

  LOG(INFO) << "exe path: " << exe_path;
  LOG(INFO) << "cache dir: " << h_cache_dir;
  LOG(INFO) << "data dir: " << h_data_dir;

  LOG(INFO) << "yass: init";

  CRYPTO_library_init();

  config::ReadConfigFileAndArguments(0, nullptr);

  g_worker = std::make_unique<Worker>();

  return nullptr;
}

static napi_value destroyRoutine(napi_env env, napi_callback_info info) {
  LOG(INFO) << "yass: deinit";

  g_worker.reset();

  return nullptr;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      {"setProtectFdCallback", nullptr, setProtectFdCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setProtectFdCallbackCleanup", nullptr, setProtectFdCallbackCleanup, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"initTun2proxy", nullptr, initTun2proxy, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"runTun2proxy", nullptr, runTun2proxy, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"stopTun2proxy", nullptr, stopTun2proxy, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"startWorker", nullptr, startWorker, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"stopWorker", nullptr, stopWorker, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getTransferRate", nullptr, getTransferRate, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"saveConfig", nullptr, saveConfig, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getServerHost", nullptr, getServerHost, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getServerSNI", nullptr, getServerSNI, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getServerPort", nullptr, getServerPort, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getUsername", nullptr, getUsername, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPassword", nullptr, getPassword, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getCipher", nullptr, getCipher, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getCipherStrings", nullptr, getCipherStrings, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getDoHUrl", nullptr, getDoHUrl, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getDoTHost", nullptr, getDoTHost, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getTimeout", nullptr, getTimeout, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"init", nullptr, initRoutine, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"destroy", nullptr, destroyRoutine, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  napi_define_properties(env, exports, std::size(desc), desc);
  return exports;
}

static napi_module yassModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  napi_module_register(&yassModule);
}

#endif  // __OHOS__
