// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#ifdef __OHOS__

#include "harmony/yass.hpp"

#include "core/logging.hpp"

#include <napi/native_api.h>
#include <js_native_api.h>
#include <js_native_api_types.h>

static napi_value setProtectFdCallbackThis = nullptr;
static napi_value setProtectFdCallbackFunc = nullptr;

static napi_value setProtectFdCallback(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 1;
  napi_value args[1] = {nullptr};
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_cb_info: " << status;
    return 0;
  }

  napi_value cb = args[0];
  napi_valuetype type;
  status = napi_typeof(env, cb, &type);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_typeof: " << status;
    return 0;
  }

  if (type != napi_function) {
    LOG(WARNING) << "invalid type: " << type << " expected: napi_function";
    return 0;
  }

  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) {
    LOG(WARNING) << "napi_get_global: " << status;
    return 0;
  }

  setProtectFdCallbackThis = global;
  setProtectFdCallbackFunc = cb;

  return 0;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
    {"setProtectFdCallback", nullptr, setProtectFdCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
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
