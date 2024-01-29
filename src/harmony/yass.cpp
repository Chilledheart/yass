// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#ifdef __OHOS__

#include "harmony/yass.hpp"

#include <base/posix/eintr_wrapper.h>
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <napi/native_api.h>

#include <iomanip>
#include <memory>
#include <thread>
#include <unistd.h>

#include "cli/cli_worker.hpp"
#include "cli/cli_connection_stats.hpp"
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

extern "C"
bool OH_LOG_IsLoggable(unsigned int domain, const char *tag, HILOG_LogLevel level);

static constexpr char kLogTag[] = YASS_APP_NAME;
static constexpr unsigned int kLogDomain = 0x15b0;

static napi_env setProtectFdCallbackEnv = nullptr;
static napi_threadsafe_function setProtectFdCallbackFunc = nullptr;

struct AsyncProtectFdEx_t {
  napi_async_work work_item;
  int fd;
  int write_end;
};

struct SetProtectFdEx_t {
  int fd;
  int write_end;
};

static constexpr char kAsyncResourceName[] = "Thread-safe SetProtectFd";

static void setProtectFdWriteResult(int fd, napi_status status) {
  int result = status;
  if (HANDLE_EINTR(write(fd, &result, sizeof(result))) < 0) {
    PLOG(WARNING) << "write failed to pipe";
  }
}

static void setProtectFdExecuteWork(napi_env env, void *data) {
  AsyncProtectFdEx_t *async_protect_fd_ex = (AsyncProtectFdEx_t *)data;

  auto ctx = new SetProtectFdEx_t;
  if (ctx == nullptr) {
    LOG(WARNING) << "failed to allocate new ctx";
    setProtectFdWriteResult(async_protect_fd_ex->fd, napi_queue_full);
    return;
  }

  ctx->fd = async_protect_fd_ex->fd;
  ctx->write_end = async_protect_fd_ex->write_end;

  auto status = napi_acquire_threadsafe_function(setProtectFdCallbackFunc);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_acquire_threadsafe_function: " << status;
    setProtectFdWriteResult(async_protect_fd_ex->fd, status);
    return;
  }

  status = napi_call_threadsafe_function(setProtectFdCallbackFunc, ctx, napi_tsfn_blocking);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_call_threadsafe_function: " << status;
    setProtectFdWriteResult(async_protect_fd_ex->fd, status);
    return;
  }

  status = napi_release_threadsafe_function(setProtectFdCallbackFunc, napi_tsfn_release);

  if (status != napi_ok) {
    LOG(WARNING) << "napi_release_threadsafe_function: " << status;
    setProtectFdWriteResult(async_protect_fd_ex->fd, status);
    return;
  }
}

// This function runs on the main thread after `ExecuteWork` exited.
static void setProtectFdOnWorkComplete(napi_env env, napi_status status, void *data) {
  AsyncProtectFdEx_t *async_protect_fd_ex = (AsyncProtectFdEx_t *)data;

  status = napi_delete_async_work(env, async_protect_fd_ex->work_item);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_delete_async_work: " << status;
    return;
  }

  delete async_protect_fd_ex;
}

int setProtectFd(int fd) {
  napi_env env = setProtectFdCallbackEnv;
  if (env == nullptr) {
    LOG(WARNING) << "null env";
    return -1;
  }

  napi_value work_name;
  // Specify a name to describe this asynchronous operation.
  auto status = napi_create_string_utf8(env, kAsyncResourceName, NAPI_AUTO_LENGTH, &work_name);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_create_string_utf8: " << status;
    return -1;
  }
  AsyncProtectFdEx_t *async_protect_fd_ex = new AsyncProtectFdEx_t;
  napi_async_work work_item;

  status = napi_create_async_work(env, nullptr, work_name,
    setProtectFdExecuteWork, setProtectFdOnWorkComplete, async_protect_fd_ex,
    &work_item); // OUT: THE handle to the async work item
  if (status != napi_ok) {
    delete async_protect_fd_ex;
    LOG(WARNING) << "napi_create_async_work: " << status;
    return -1;
  }

  int pipefd[2] = {-1, -1};
  if (pipe(pipefd) == -1) {
    PLOG(WARNING) << "create pipe failed";
    return -1;
  };
  int read_end = pipefd[0];
  async_protect_fd_ex->work_item = work_item;
  async_protect_fd_ex->fd = fd;
  async_protect_fd_ex->write_end = pipefd[1];

  // Queue the work item for execution.
  status = napi_queue_async_work(env, work_item);
  if (status != napi_ok) {
    IGNORE_EINTR(close(pipefd[0]));
    IGNORE_EINTR(close(pipefd[1]));
    LOG(WARNING) << "napi_create_async_work: " << status;
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

static void setProtectFdCallingJS(napi_env env, napi_value js_cb, void *context, void *data) {
  // This parameter is not used.
  (void)context;

  auto ctx = reinterpret_cast<SetProtectFdEx_t*>(data);
  int fd_value = ctx->fd;
  int write_end = ctx->write_end;
  delete ctx;

  if (env == nullptr) {
    LOG(WARNING) << "null env";
    return;
  }

  napi_value undefined;
  napi_status status = napi_get_undefined(env, &undefined);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_undefined: " << status;
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

  napi_value argv[] = { fd, callback };
  status = napi_call_function(env, undefined, js_cb, std::size(argv), argv, nullptr);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_call_function: " << status;
    setProtectFdWriteResult(fd_value, status);
    return;
  }
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

  napi_value work_name;
  // Specify a name to describe this asynchronous operation.
  status = napi_create_string_utf8(env, kAsyncResourceName, NAPI_AUTO_LENGTH, &work_name);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  // Create a thread-safe N-API callback function correspond to the C/C++ callback function
  status = napi_create_threadsafe_function(env, cb, nullptr, work_name, 0, 1, nullptr,
      nullptr, nullptr, setProtectFdCallingJS, // the C/C++ callback function
      &setProtectFdCallbackFunc // out: the asynchronous thread-safe JavaScript function
  );
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_threadsafe_function failed");
    return nullptr;
  }

  setProtectFdCallbackEnv = env;

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
static napi_value startTun2proxy(napi_env env, napi_callback_info info) {
  napi_value args[4] {};
  size_t argc = std::size(args);
  auto status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok || argc != 4) {
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

  int log_level = 0; // stands for rust log level, 0 stands for no log output

  if (OH_LOG_IsLoggable(kLogDomain, kLogTag, HILOG_LOG_DEBUG)) {
    log_level = 5; // TRACE
  } else if (OH_LOG_IsLoggable(kLogDomain, kLogTag, HILOG_LOG_DEBUG)) {
    log_level = 4; // DEBUG
  } else if (OH_LOG_IsLoggable(kLogDomain, kLogTag, HILOG_LOG_INFO)) {
    log_level = 3; // INFO
  } else if (OH_LOG_IsLoggable(kLogDomain, kLogTag, HILOG_LOG_WARN)) {
    log_level = 2; // WARN
  } else if (OH_LOG_IsLoggable(kLogDomain, kLogTag, HILOG_LOG_ERROR)) {
    log_level = 1; // ERROR
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

  int ret = tun2proxy_init(proxy_url_buf, tun_fd_int, tun_mtu_int, log_level, dns_over_tcp_int);
  if (ret != 0) {
    LOG(WARNING) << "tun2proxy_run failed: " << ret;
  }
  return nullptr;
}

static napi_value runTun2proxy(napi_env env, napi_callback_info info) {
  g_tun2proxy_thread = std::make_unique<std::thread>([]{
    if (!SetCurrentThreadName("tun2proxy")) {
      PLOG(WARNING) << "failed to set thread name";
    }
    if (!SetCurrentThreadPriority(ThreadPriority::ABOVE_NORMAL)) {
      PLOG(WARNING) << "failed to set thread priority";
    }
    int ret = tun2proxy_run();
    if (ret != 0) {
      LOG(WARNING) << "tun2proxy_run failed: " << ret;
    }
  });
  return nullptr;
}

static napi_value stopTun2proxy(napi_env env, napi_callback_info info) {
  if (!g_tun2proxy_thread) {
    return nullptr;
  }
  int ret = tun2proxy_destroy();
  if (ret != 0) {
    LOG(WARNING) << "tun2proxy_destroy failed: " << ret;
  }
  g_tun2proxy_thread->join();
  return nullptr;
}

static constexpr char kAsyncStartWorkerResourceName[] = "Thread-safe StartWorker";

struct AsyncStartWorkerEx_t {
  napi_threadsafe_function ts_func;
  napi_async_work work_item;
  int error_code;
};

static void startWorkerCallingJS(napi_env env, napi_value js_cb, void *context, void *data) {
  // This parameter is not used.
  (void)context;

  int err_code = reinterpret_cast<uintptr_t>(data);

  if (env == nullptr) {
    LOG(WARNING) << "null env";
    return;
  }

  napi_value undefined;
  napi_status status = napi_get_undefined(env, &undefined);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_undefined: " << status;
    return;
  }

  napi_value error_code;
  status = napi_create_int32(env, err_code, &error_code);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_create_int32: " << status;
    return;
  }

  napi_value argv[] = { error_code };
  status = napi_call_function(env, undefined, js_cb, std::size(argv), argv, nullptr);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_call_function: " << status;
    return;
  }
}

static void startWorkerExecuteWork(napi_env env, void *data) {
  AsyncStartWorkerEx_t *async_start_worker_ex = (AsyncStartWorkerEx_t *)data;
  napi_threadsafe_function startWorkerCallbackFunc = async_start_worker_ex->ts_func;

  void* ctx = (void*)(uintptr_t)async_start_worker_ex->error_code;

  auto status = napi_acquire_threadsafe_function(startWorkerCallbackFunc);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_acquire_threadsafe_function: " << status;
    return;
  }

  status = napi_call_threadsafe_function(startWorkerCallbackFunc, ctx, napi_tsfn_blocking);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_call_threadsafe_function: " << status;
    return;
  }

  status = napi_release_threadsafe_function(startWorkerCallbackFunc, napi_tsfn_release);

  if (status != napi_ok) {
    LOG(WARNING) << "napi_release_threadsafe_function: " << status;
    return;
  }
}

// This function runs on the main thread after `ExecuteWork` exited.
static void startWorkerOnWorkComplete(napi_env env, napi_status status, void *data) {
  AsyncStartWorkerEx_t *async_start_worker_ex = (AsyncStartWorkerEx_t *)data;
  napi_async_work work_item = async_start_worker_ex->work_item;
  napi_threadsafe_function startWorkerCallbackFunc = async_start_worker_ex->ts_func;
  delete async_start_worker_ex;

  status = napi_delete_async_work(env, work_item);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_delete_async_work: " << status;
  }

  // Clean up the thread-safe function and the work item associated with this
  status = napi_release_threadsafe_function(startWorkerCallbackFunc, napi_tsfn_release);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_release_threadsafe_function: " << status;
  }
}

static Worker g_worker;

static napi_value startWorker(napi_env env, napi_callback_info info) {
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

  napi_value work_name;
  // Specify a name to describe this asynchronous operation.
  status = napi_create_string_utf8(env, kAsyncStartWorkerResourceName, NAPI_AUTO_LENGTH, &work_name);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  napi_threadsafe_function startWorkerCallbackFunc;

  // Create a thread-safe N-API callback function correspond to the C/C++ callback function
  status = napi_create_threadsafe_function(env, cb, nullptr, work_name, 0, 1, nullptr,
      nullptr, nullptr, startWorkerCallingJS, // the C/C++ callback function
      &startWorkerCallbackFunc // out: the asynchronous thread-safe JavaScript function
  );
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_threadsafe_function failed");
    return nullptr;
  }

  g_worker.Start([env, startWorkerCallbackFunc](asio::error_code ec) {
    AsyncStartWorkerEx_t *async_start_worker_ex = new AsyncStartWorkerEx_t;
    napi_async_work work_item;

    napi_value work_name;
    // Specify a name to describe this asynchronous operation.
    auto status = napi_create_string_utf8(env, kAsyncStartWorkerResourceName, NAPI_AUTO_LENGTH, &work_name);
    if (status != napi_ok) {
      LOG(WARNING) << "napi_create_string_utf8: " << status;
      return;
    }

    status = napi_create_async_work(env, nullptr, work_name,
      startWorkerExecuteWork, startWorkerOnWorkComplete, async_start_worker_ex,
      &work_item); // OUT: THE handle to the async work item
    if (status != napi_ok) {
      delete async_start_worker_ex;
      LOG(WARNING) << "napi_create_async_work: " << status;
      return;
    }

    async_start_worker_ex->ts_func = startWorkerCallbackFunc;
    async_start_worker_ex->work_item = work_item;
    async_start_worker_ex->error_code = ec.value();

    // Queue the work item for execution.
    status = napi_queue_async_work(env, work_item);
    if (status != napi_ok) {
      delete async_start_worker_ex; // TODO better cleanup?
      LOG(WARNING) << "napi_create_async_work: " << status;
      return;
    }
  });
  return nullptr;
}

static constexpr char kAsyncStopWorkerResourceName[] = "Thread-safe StopWorker";

struct AsyncStopWorkerEx_t {
  napi_threadsafe_function ts_func;
  napi_async_work work_item;
};

static void stopWorkerCallingJS(napi_env env, napi_value js_cb, void *context, void *data) {
  // This parameter is not used.
  (void)context;

  if (env == nullptr) {
    LOG(WARNING) << "null env";
    return;
  }

  napi_value undefined;
  napi_status status = napi_get_undefined(env, &undefined);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_undefined: " << status;
    return;
  }

  status = napi_call_function(env, undefined, js_cb, 0, nullptr, nullptr);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_call_function: " << status;
    return;
  }
}

static void stopWorkerExecuteWork(napi_env env, void *data) {
  AsyncStopWorkerEx_t *async_stop_worker_ex = (AsyncStopWorkerEx_t *)data;
  napi_threadsafe_function stopWorkerCallbackFunc = async_stop_worker_ex->ts_func;

  void* ctx = nullptr;

  auto status = napi_acquire_threadsafe_function(stopWorkerCallbackFunc);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_acquire_threadsafe_function: " << status;
    return;
  }

  status = napi_call_threadsafe_function(stopWorkerCallbackFunc, ctx, napi_tsfn_blocking);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_call_threadsafe_function: " << status;
    return;
  }

  status = napi_release_threadsafe_function(stopWorkerCallbackFunc, napi_tsfn_release);

  if (status != napi_ok) {
    LOG(WARNING) << "napi_release_threadsafe_function: " << status;
    return;
  }
}

// This function runs on the main thread after `ExecuteWork` exited.
static void stopWorkerOnWorkComplete(napi_env env, napi_status status, void *data) {
  AsyncStopWorkerEx_t *async_stop_worker_ex = (AsyncStopWorkerEx_t *)data;
  napi_async_work work_item = async_stop_worker_ex->work_item;
  napi_threadsafe_function stopWorkerCallbackFunc = async_stop_worker_ex->ts_func;
  delete async_stop_worker_ex;

  status = napi_delete_async_work(env, work_item);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_delete_async_work: " << status;
  }

  // Clean up the thread-safe function and the work item associated with this
  status = napi_release_threadsafe_function(stopWorkerCallbackFunc, napi_tsfn_release);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_release_threadsafe_function: " << status;
  }
}

static napi_value stopWorker(napi_env env, napi_callback_info info) {
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

  napi_value work_name;
  // Specify a name to describe this asynchronous operation.
  status = napi_create_string_utf8(env, kAsyncStopWorkerResourceName, NAPI_AUTO_LENGTH, &work_name);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  napi_threadsafe_function stopWorkerCallbackFunc;

  // Create a thread-safe N-API callback function correspond to the C/C++ callback function
  status = napi_create_threadsafe_function(env, cb, nullptr, work_name, 0, 1, nullptr,
      nullptr, nullptr, stopWorkerCallingJS, // the C/C++ callback function
      &stopWorkerCallbackFunc // out: the asynchronous thread-safe JavaScript function
  );
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_threadsafe_function failed");
    return nullptr;
  }

  g_worker.Stop([env, stopWorkerCallbackFunc]() {
    AsyncStopWorkerEx_t *async_stop_worker_ex = new AsyncStopWorkerEx_t;
    napi_async_work work_item;

    napi_value work_name;
    // Specify a name to describe this asynchronous operation.
    auto status = napi_create_string_utf8(env, kAsyncStopWorkerResourceName, NAPI_AUTO_LENGTH, &work_name);
    if (status != napi_ok) {
      LOG(WARNING) << "napi_create_string_utf8: " << status;
      return;
    }

    status = napi_create_async_work(env, nullptr, work_name,
      stopWorkerExecuteWork, stopWorkerOnWorkComplete, async_stop_worker_ex,
      &work_item); // OUT: THE handle to the async work item
    if (status != napi_ok) {
      delete async_stop_worker_ex;
      LOG(WARNING) << "napi_create_async_work: " << status;
      return;
    }

    async_stop_worker_ex->ts_func = stopWorkerCallbackFunc;
    async_stop_worker_ex->work_item = work_item;

    // Queue the work item for execution.
    status = napi_queue_async_work(env, work_item);
    if (status != napi_ok) {
      delete async_stop_worker_ex; // TODO better cleanup?
      LOG(WARNING) << "napi_create_async_work: " << status;
      return;
    }
  });
  return nullptr;
}

static void humanReadableByteCountBin(std::ostream* ss, uint64_t bytes) {
  if (bytes < 1024) {
    *ss << bytes << " B";
    return;
  }
  uint64_t value = bytes;
  char ci[] = {"KMGTPE"};
  const char* c = ci;
  for (int i = 40; i >= 0 && bytes > 0xfffccccccccccccLU >> i; i -= 10) {
    value >>= 10;
    ++c;
  }
  *ss << std::fixed << std::setw(5) << std::setprecision(2) << value / 1024.0
      << " " << *c;
}

static uint64_t g_last_sync_time;
static uint64_t g_last_tx_bytes;
static uint64_t g_last_rx_bytes;

// return { rx_rate, tx_rate }
static napi_value getTransferRate(napi_env env, napi_callback_info info) {
  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - g_last_sync_time;
  static uint64_t rx_rate = 0;
  static uint64_t tx_rate = 0;
  if (delta_time > NS_PER_SECOND) {
    uint64_t rx_bytes = cli::total_rx_bytes;
    uint64_t tx_bytes = cli::total_tx_bytes;
    rx_rate = static_cast<double>(rx_bytes - g_last_rx_bytes) / delta_time *
               NS_PER_SECOND;
    tx_rate = static_cast<double>(tx_bytes - g_last_tx_bytes) / delta_time *
               NS_PER_SECOND;
    g_last_sync_time = sync_time;
    g_last_rx_bytes = rx_bytes;
    g_last_tx_bytes = tx_bytes;
  }

  std::stringstream rx_ss, tx_ss;
  humanReadableByteCountBin(&rx_ss, rx_rate);
  rx_ss << "/s";

  napi_value rx_rate_value;
  auto status = napi_create_string_utf8(env, rx_ss.str().c_str(), NAPI_AUTO_LENGTH, &rx_rate_value);
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "napi_create_string_utf8 failed");
    return nullptr;
  }

  humanReadableByteCountBin(&tx_ss, tx_rate);
  tx_ss << "/s";

  napi_value tx_rate_value;
  status = napi_create_string_utf8(env, tx_ss.str().c_str(), NAPI_AUTO_LENGTH, &tx_rate_value);
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

  return results;
}

// Passing argument in a array
// server_host
// server_sni
// server_port
// username
// password
// method
// timeout
static napi_value saveConfig(napi_env env, napi_callback_info info) {

  napi_value args[7] {};
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
  std::string timeout = argList[6];

  std::string local_host = "0.0.0.0";
  std::string local_port = "0";

  std::string err_msg = config::ReadConfigFromArgument(server_host, server_sni, server_port,
                                                       username, password, method,
                                                       local_host, local_port, timeout);

  if (err_msg.empty()) {
    return nullptr;
  }

  // should me use throw?
  napi_throw_error(env, nullptr, err_msg.c_str());
  return nullptr;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
    {"setProtectFdCallback", nullptr, setProtectFdCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"setProtectFdCallbackCleanup", nullptr, setProtectFdCallbackCleanup, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"startTun2proxy", nullptr, startTun2proxy, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"runTun2proxy", nullptr, runTun2proxy, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"stopTun2proxy", nullptr, stopTun2proxy, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"startWorker", nullptr, startWorker, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"stopWorker", nullptr, stopWorker, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"getTransferRate", nullptr, getTransferRate, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"saveConfig", nullptr, saveConfig, nullptr, nullptr, nullptr, napi_default, nullptr},
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
  .nm_priv = ((void *)0),
  .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) {
  napi_module_register(&yassModule);
}

#endif // __OHOS__
