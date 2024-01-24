// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#ifdef __OHOS__

#include "harmony/yass.hpp"

#include "core/logging.hpp"
#include <base/posix/eintr_wrapper.h>

#include <napi/native_api.h>
#include <js_native_api.h>
#include <js_native_api_types.h>

#include <unistd.h>

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
  if (status != napi_ok) {
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

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
    {"setProtectFdCallback", nullptr, setProtectFdCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"setProtectFdCallbackCleanup", nullptr, setProtectFdCallbackCleanup, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
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
