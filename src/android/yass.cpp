// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#ifdef __ANDROID__

#include "android/yass.hpp"

#include <jni.h>
#include "third_party/boringssl/src/include/openssl/crypto.h"

#include "android/jni.hpp"
#include "android/utils.hpp"
#include "cli/cli_connection_stats.hpp"
#include "cli/cli_worker.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crashpad_helper.hpp"

namespace config {
const ProgramType pType = YASS_CLIENT_GUI;
}  // namespace config

// Data
static bool g_Initialized = false;

// Forward declarations of helper functions
static void Init(JNIEnv* env, jobject activity_obj);
static void Shutdown();

static int CallOnNativeStarted(JavaVM* jvm, jobject activity_obj, const std::string& errmsg, jint port);
static int CallOnNativeStopped(JavaVM* jvm, jobject activity_obj);

static std::unique_ptr<Worker> g_worker;

void Init(JNIEnv* env, jobject activity_obj) {
  if (g_Initialized)
    return;

  LOG(INFO) << "android: Initialize";

#ifdef HAVE_C_ARES
  CHECK_EQ(0, InitializeCares(env, activity_obj));
#endif

#ifdef HAVE_CRASHPAD
  // FIXME correct the path
  std::string lib_path;
  CHECK_EQ(0, GetNativeLibraryDirectory(env, activity_obj, &lib_path));
  CHECK(InitializeCrashpad(lib_path + "/libnative-lib.so"));
#endif

  CRYPTO_library_init();

  config::ReadConfigFileAndArguments(0, nullptr);

  // Create Main Worker after ReadConfig
  g_worker = std::make_unique<Worker>();

  g_Initialized = true;

  LOG(INFO) << "android: Initialized";
}

void Shutdown() {
  if (!g_Initialized)
    return;

  LOG(INFO) << "android: Shutdown";

  g_Initialized = false;

  g_worker.reset();

  LOG(INFO) << "android: Shutdown finished";
}

static int CallOnNativeStarted(JavaVM* jvm, jobject activity_obj, const std::string& errmsg, jint port) {
  JavaVM* java_vm = jvm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -1;

  jclass activity_clazz = java_env->GetObjectClass(activity_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id = java_env->GetMethodID(activity_clazz, "onNativeStarted", "(Ljava/lang/String;I)V");
  if (method_id == nullptr)
    return -1;

  java_env->CallVoidMethod(activity_obj, method_id, errmsg.empty() ? nullptr : java_env->NewStringUTF(errmsg.c_str()),
                           port);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -1;

  return 0;
}

static int CallOnNativeStopped(JavaVM* jvm, jobject activity_obj) {
  JavaVM* java_vm = jvm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -1;

  jclass activity_clazz = java_env->GetObjectClass(activity_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id = java_env->GetMethodID(activity_clazz, "onNativeStopped", "()V");
  if (method_id == nullptr)
    return -1;

  java_env->CallVoidMethod(activity_obj, method_id);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -1;

  return 0;
}

// called from java thread
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_onNativeCreate(JNIEnv* env, jobject obj) {
  CHECK(g_jvm) << "jvm not found";
  g_activity_obj = obj;

  a_open_apk_asset = OpenApkAsset;

  // before any log calls
  std::string cache_path;
  CHECK_EQ(0, GetCacheLibraryDirectory(env, obj, &cache_path));
  a_cache_dir = cache_path;

  std::string exe_path;
  GetExecutablePath(&exe_path);
  SetExecutablePath(exe_path);

  std::string data_path;
  CHECK_EQ(0, GetDataLibraryDirectory(env, obj, &data_path));
  a_data_dir = data_path;

  LOG(INFO) << "exe path: " << exe_path;
  LOG(INFO) << "cache dir: " << cache_path;
  LOG(INFO) << "data dir: " << data_path;

  // possible values: en_US, zh_SG_#Hans, zh_CN_#Hans, zh_HK_#Hant
  std::string locale_name;
  CHECK_EQ(0, GetCurrentLocale(env, obj, &locale_name));
  LOG(INFO) << "current locale: " << locale_name;

  Init(env, obj);

  g_activity_obj = env->NewGlobalRef(obj);
}

// called from java thread
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_onNativeDestroy(JNIEnv* env, jobject obj) {
  Shutdown();
  env->DeleteGlobalRef(g_activity_obj);
  g_activity_obj = nullptr;
}

static uint64_t g_last_sync_time = 0;
static uint64_t g_last_tx_bytes = 0;
static uint64_t g_last_rx_bytes = 0;

JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_nativeStart(JNIEnv* env, jobject obj) {
  g_worker->Start([&](asio::error_code ec) {
    if (!ec) {
      config::SaveConfig();
    }
    std::ostringstream ss;
    if (ec) {
      ss << ec;
    }
    int port = ec ? 0 : g_worker->GetLocalPort();
    CallOnNativeStarted(g_jvm, g_activity_obj, ss.str(), port);
  });
}

JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_nativeStop(JNIEnv* env, jobject obj) {
  g_worker->Stop([&]() { CallOnNativeStopped(g_jvm, g_activity_obj); });
}

JNIEXPORT jlongArray JNICALL Java_it_gui_yass_MainActivity_getRealtimeTransferRate(JNIEnv* env, jobject obj) {
  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - g_last_sync_time;
  static uint64_t rx_rate = 0;
  static uint64_t tx_rate = 0;
  if (delta_time > NS_PER_SECOND) {
    uint64_t rx_bytes = net::cli::total_rx_bytes;
    uint64_t tx_bytes = net::cli::total_tx_bytes;
    rx_rate = static_cast<double>(rx_bytes - g_last_rx_bytes) / delta_time * NS_PER_SECOND;
    tx_rate = static_cast<double>(tx_bytes - g_last_tx_bytes) / delta_time * NS_PER_SECOND;
    g_last_sync_time = sync_time;
    g_last_rx_bytes = rx_bytes;
    g_last_tx_bytes = tx_bytes;
  }
  jlong dresult[3] = {g_worker->currentConnections(), rx_rate, tx_rate};
  auto result = env->NewLongArray(3);
  env->SetLongArrayRegion(result, 0, 3, dresult);

  std::stringstream ss;
  ss << "polling " << dresult[0] << " connections";

  ss << " rx rate: ";
  HumanReadableByteCountBin(&ss, dresult[1]);
  ss << "/s";
  ss << " tx rate: ";
  HumanReadableByteCountBin(&ss, dresult[2]);
  ss << "/s";

  VLOG(1) << ss.str();
  return result;
}

#endif  // __ANDROID__
