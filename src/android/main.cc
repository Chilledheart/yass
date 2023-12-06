// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifdef __ANDROID__

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/str_format.h>
#include <openssl/crypto.h>

#include <atomic>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include "core/logging.hpp"
#include "i18n/icu_util.hpp"
#include "core/utils.hpp"
#include "cli/cli_worker.hpp"

/*
 * AcquireASensorManagerInstance(void)
 *    Workaround ASensorManager_getInstance() deprecation false alarm
 *    for Android-N and before, when compiling with NDK-r15
 */
#include <dlfcn.h>
static ASensorManager* AcquireASensorManagerInstance(android_app* app) {
  if (!app) return nullptr;

  typedef ASensorManager* (*PF_GETINSTANCEFORPACKAGE)(const char* name);
  void* androidHandle = dlopen("libandroid.so", RTLD_NOW);
  auto getInstanceForPackageFunc = (PF_GETINSTANCEFORPACKAGE)dlsym(
      androidHandle, "ASensorManager_getInstanceForPackage");
  if (getInstanceForPackageFunc) {
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    jclass android_content_Context = env->GetObjectClass(app->activity->clazz);
    jmethodID midGetPackageName = env->GetMethodID(
        android_content_Context, "getPackageName", "()Ljava/lang/String;");
    auto packageName =
        (jstring)env->CallObjectMethod(app->activity->clazz, midGetPackageName);

    const char* nativePackageName =
        env->GetStringUTFChars(packageName, nullptr);
    ASensorManager* mgr = getInstanceForPackageFunc(nativePackageName);
    env->ReleaseStringUTFChars(packageName, nativePackageName);
    app->activity->vm->DetachCurrentThread();
    if (mgr) {
      dlclose(androidHandle);
      return mgr;
    }
  }

  typedef ASensorManager* (*PF_GETINSTANCE)();
  auto getInstanceFunc =
      (PF_GETINSTANCE)dlsym(androidHandle, "ASensorManager_getInstance");
  // by all means at this point, ASensorManager_getInstance should be available
  assert(getInstanceFunc);
  dlclose(androidHandle);

  return getInstanceFunc();
}

static void WorkFunc() {
  Worker worker;
  enum StartState {
    STOPPED = 0,
    STOPPING,
    STARTING,
    STARTED,
  };
  std::atomic<StartState> state(STOPPED);

  // Prepare to monitor accelerometer
  auto sensorManager = AcquireASensorManagerInstance(a_app);
  auto accelerometerSensor = ASensorManager_getDefaultSensor(
      sensorManager, ASENSOR_TYPE_ACCELEROMETER);
  auto sensorEventQueue = ASensorManager_createEventQueue(
      sensorManager, a_app->looper, LOOPER_ID_USER, nullptr, nullptr);

  LOG(WARNING) << "sensorManager: " << sensorManager;

  while (1) {
    // Read all pending events.
    int timeoutMs = -1;
    int ident;
    int events;
    struct android_poll_source* source;

    while ((ident=ALooper_pollAll(timeoutMs, nullptr, &events,
                                  (void**)&source)) >= 0) {

      // Process this event.
      if (source != nullptr) {
        source->process(a_app, source);
      }

      if (ident == LOOPER_ID_USER) {
        LOG(INFO) << "LOOPER_ID_USER";
        if (accelerometerSensor != nullptr) {
          ASensorEvent event;
          while (ASensorEventQueue_getEvents(sensorEventQueue, &event, 1) > 0) {
            LOG(INFO) << absl::StrFormat("accelerometer: x=%f y=%f z=%f",
                                         event.acceleration.x,
                                         event.acceleration.y,
                                         event.acceleration.z);
          }
        }
        switch(state) {
          case STOPPED:
            state = STARTING;
            worker.Start([&](asio::error_code ec) {
              if (ec) {
                LOG(WARNING) << "Start Failed: " << ec;
              } else {
                LOG(WARNING) << "Started";
                state = STARTED;
              }
            });
            break;
          case STOPPING:
            LOG(WARNING) << "Stopping, please wait";
            break;
          case STARTING:
            LOG(WARNING) << "Starting, please wait";
            break;
          case STARTED:
            state = STOPPING;
            worker.Stop([&]() {
              LOG(WARNING) << "Stopped";
              state = STOPPED;
            });
            break;
        }
      }

      // Check if we are exiting.
      if (a_app->destroyRequested != 0) {
        return;
      }
    }
  }
}

void android_main(android_app* state) {
#ifdef HAVE_ICU
  if (!InitializeICU()) {
    LOG(WARNING) << "Failed to initialize icu component";
    return;
  }
#endif

  CRYPTO_library_init();

  a_app = state;

  WorkFunc();
}

#endif // __ANDROID__
