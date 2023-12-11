// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifdef __ANDROID__

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/str_format.h>
#include <base/posix/eintr_wrapper.h>
#include <openssl/crypto.h>

#include <android/log.h>
#include <atomic>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <deque>

#include "core/logging.hpp"
#include "i18n/icu_util.hpp"
#include "core/utils.hpp"
#include "cli/cli_worker.hpp"
#include "config/config.hpp"
#include "crashpad_helper.hpp"

// Data
static std::atomic_bool     g_Initialized = false;
static JavaVM              *g_jvm = nullptr;
static JNIEnv              *g_env = nullptr;
static jobject              g_obj = nullptr;

// Forward declarations of helper functions
static void Init(JNIEnv *env);
static void Shutdown();
// returning in host byte order
[[nodiscard]] [[maybe_unused]]
static int32_t GetIpAddress(JNIEnv *env);
[[maybe_unused]]
static int SetJavaThreadName(const std::string& thread_name);
[[maybe_unused]]
static int GetJavaThreadName(std::string* thread_name);
[[maybe_unused]]
static int GetNativeLibraryDirectory(JNIEnv *env, std::string* result);
[[maybe_unused]]
static int GetCacheLibraryDirectory(JNIEnv *env, std::string* result);
[[maybe_unused]]
static int GetDataLibraryDirectory(JNIEnv *env, std::string* result);
[[maybe_unused]]
static int GetCurrentLocale(JNIEnv *env, std::string* result);
[[maybe_unused]]
static int OpenApkAsset(const std::string& file_path,
                        gurl_base::MemoryMappedFile::Region* region);

extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_onNativeCreate(JNIEnv *env, jobject obj);
extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_onNativeDestroy(JNIEnv *env, jobject obj);
extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_notifyUnicodeChar(JNIEnv *env, jobject obj, jint unicode);

static std::unique_ptr<Worker> g_worker;

enum StartState {
  STOPPED = 0,
  STOPPING,
  STARTING,
  STARTED,
};

static std::atomic<StartState> g_state(STOPPED);

#if 0
static const char* state_to_str(StartState state) {
  switch(state) {
    case STOPPED:
      return "STOPPED";
    case STOPPING:
      return "STOPPING";
    case STARTING:
      return "STARTING";
    case STARTED:
      return "STARTED";
  }
}

static void ToggleWorker() {
  switch(g_state) {
    case STOPPED:
      g_state = STARTING;
      g_worker->Start([&](asio::error_code ec) {
        if (ec) {
          LOG(WARNING) << "Start Failed: " << ec;
          g_state = STOPPED;
        } else {
          LOG(WARNING) << "Started";
          config::SaveConfig();
          g_state = STARTED;
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
      g_state = STOPPING;
      g_worker->Stop([&]() {
        LOG(WARNING) << "Stopped";
        g_state = STOPPED;
      });
      break;
  }
}
#endif

void Init(JNIEnv *env) {
  if (g_Initialized)
    return;

  LOG(INFO) << "android: Initialize";

#ifdef HAVE_CRASHPAD
  // FIXME correct the path
  std::string lib_path;
  CHECK_EQ(0, GetNativeLibraryDirectory(env, &lib_path));
  CHECK(InitializeCrashpad(lib_path + "/libnative-lib.so"));
#endif

#ifdef HAVE_ICU
  if (!InitializeICU()) {
    LOG(WARNING) << "Failed to initialize icu component";
    return;
  }
#endif

  CRYPTO_library_init();

  config::ReadConfigFileOption(0, nullptr);
  config::ReadConfig();

  // Create Main Worker after ReadConfig
  g_worker = std::make_unique<Worker>();

  g_Initialized = true;

  LOG(INFO) << "android: Initialized";
}

#if 0
void MainLoopStep()
{
  {
    std::vector<const char*> methods = {
#define XX(num, name, string) CRYPTO_##name##_STR,
CIPHER_METHOD_VALID_MAP(XX)
#undef XX
    };

    std::vector<cipher_method> methods_idxes = {
#define XX(num, name, string) CRYPTO_##name,
CIPHER_METHOD_VALID_MAP(XX)
#undef XX
    };
    static char server_host[140];
    static int server_port;
    static char username[140];
    static char password[140];
    static int method_idx = [&](cipher_method method) -> int {
      auto it = std::find(methods_idxes.begin(), methods_idxes.end(), method);
      if (it == methods_idxes.end()) {
        return 0;
      }
      return it - methods_idxes.begin();
    }(absl::GetFlag(FLAGS_method).method);
    static char local_host[140];
    static int local_port;
    static int timeout;
    static std::string ipaddress = asio::ip::make_address_v4(GetIpAddress()).to_string();
    static bool _init = [&]() -> bool {
      strcpy(server_host, absl::GetFlag(FLAGS_server_host).c_str());
      server_port = absl::GetFlag(FLAGS_server_port);
      strcpy(username, absl::GetFlag(FLAGS_username).c_str());
      strcpy(password, absl::GetFlag(FLAGS_password).c_str());
      strcpy(local_host, absl::GetFlag(FLAGS_local_host).c_str());
      local_port = absl::GetFlag(FLAGS_local_port);
      timeout = absl::GetFlag(FLAGS_connect_timeout);

      // FIXME the thread don't set correctly if it is done after
      // GetIpAddress call
      SetJavaThreadName("Native Thread");
      return true;
    }();

    ImGui::Begin("Yet Another Shadow Socket");

    ImGui::InputText("Server Host", server_host, IM_ARRAYSIZE(server_host));
    ImGui::InputInt("Server Port", &server_port);
    ImGui::InputText("Username", username, IM_ARRAYSIZE(username));
    ImGui::InputText("Password", password, IM_ARRAYSIZE(password),
                     ImGuiInputTextFlags_Password);

    ImGui::ListBox("Cipher", &method_idx,
                   &methods[0], static_cast<int>(methods.size()), 2);
    ImGui::InputText("Local Host", local_host, IM_ARRAYSIZE(local_host));
    ImGui::InputInt("Local Port", &local_port);
    ImGui::InputInt("Timeout", &timeout);
    ImGui::Text("Current Ip Address: %s", ipaddress.c_str());

    ImGui::Checkbox("Option Window", &show_option_window);

    if (ImGui::Button("Toggle Service")) {
      absl::SetFlag(&FLAGS_server_host, server_host);
      absl::SetFlag(&FLAGS_server_port, server_port);
      absl::SetFlag(&FLAGS_username, username);
      absl::SetFlag(&FLAGS_password, password);
      absl::SetFlag(&FLAGS_method, methods_idxes[method_idx]);
      absl::SetFlag(&FLAGS_local_host, local_host);
      absl::SetFlag(&FLAGS_local_port, local_port);
      absl::SetFlag(&FLAGS_connect_timeout, timeout);

      ToggleWorker();
    }
    ImGui::SameLine();
    ImGui::Text("Status = %s", state_to_str(g_state));

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
  }

  if (show_option_window)
  {
    static bool tcp_keep_alive = absl::GetFlag(FLAGS_tcp_keep_alive);
    static int tcp_keep_alive_cnt = absl::GetFlag(FLAGS_tcp_keep_alive_cnt);
    static int tcp_keep_alive_idle_timeout = absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout);
    static int tcp_keep_alive_interval = absl::GetFlag(FLAGS_tcp_keep_alive_interval);

    ImGui::Begin("Options Window", &show_option_window);
    ImGui::Checkbox("TCP keep alive", &tcp_keep_alive);
    ImGui::InputInt("The number of TCP keep-alive probes", &tcp_keep_alive_cnt);
    ImGui::InputInt("TCP keep alive after idle", &tcp_keep_alive_idle_timeout);
    ImGui::InputInt("TCP keep alive interval", &tcp_keep_alive_interval);
    if (ImGui::Button("Close Me")) {
      absl::SetFlag(&FLAGS_tcp_keep_alive, tcp_keep_alive);
      absl::SetFlag(&FLAGS_tcp_keep_alive_cnt, tcp_keep_alive_cnt);
      absl::SetFlag(&FLAGS_tcp_keep_alive_idle_timeout, tcp_keep_alive_idle_timeout);
      absl::SetFlag(&FLAGS_tcp_keep_alive_interval, tcp_keep_alive_interval);

      show_option_window = false;
    }
    ImGui::End();
  }
}
#endif

void Shutdown()
{
  if (!g_Initialized)
    return;
  LOG(INFO) << "android: Shutdown";

  g_Initialized = false;

  g_worker.reset();

  LOG(INFO) << "android: Shutdown finished";
}

static int32_t GetIpAddress(JNIEnv *env)
{
  JavaVM* java_vm = g_jvm;

  jclass activity_clazz = env->GetObjectClass(g_obj);
  if (activity_clazz == nullptr)
    return 0;

  jmethodID method_id = env->GetMethodID(activity_clazz, "getIpAddress", "()I");
  if (method_id == nullptr)
    return 0;

  jint ip_address = env->CallIntMethod(g_obj, method_id);

  return ntohl(ip_address);
}

// Set native thread name inside
int SetJavaThreadName(const std::string& thread_name) {
  JavaVM* java_vm = g_jvm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass thread_clazz = java_env->FindClass("java/lang/Thread");
  if (thread_clazz == nullptr )
    return -3;

  jmethodID method_id = java_env->GetStaticMethodID(thread_clazz, "currentThread", "()Ljava/lang/Thread;");
  if (method_id == nullptr)
    return -4;

  jobject thread_obj = java_env->CallStaticObjectMethod(thread_clazz, method_id);

  if (thread_obj == nullptr)
    return -5;

  jstring thread_name_obj = java_env->NewStringUTF(thread_name.c_str());
  if (thread_name_obj == nullptr)
    return -6;

  jmethodID set_method_id = java_env->GetMethodID(thread_clazz, "setName", "(Ljava/lang/String;)V");
  if (set_method_id == nullptr)
    return -7;

  java_env->CallVoidMethod(thread_obj, set_method_id, thread_name_obj);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -8;

  return 0;
}

int GetJavaThreadName(std::string* thread_name) {
  JavaVM* java_vm = g_jvm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass thread_clazz = java_env->FindClass("java/lang/Thread");
  if (thread_clazz == nullptr )
    return -3;

  jmethodID method_id = java_env->GetStaticMethodID(thread_clazz, "currentThread", "()Ljava/lang/Thread;");
  if (method_id == nullptr)
    return -4;

  jobject thread_obj = java_env->CallStaticObjectMethod(thread_clazz, method_id);

  if (thread_obj == nullptr)
    return -5;

  jmethodID get_method_id = java_env->GetMethodID(thread_clazz, "getName", "()Ljava/lang/String;");
  if (get_method_id == nullptr)
    return -6;

  jobject name = java_env->CallObjectMethod(thread_obj, get_method_id);
  if (name == nullptr)
    return -7;

  const char* name_str;
  name_str = java_env->GetStringUTFChars((jstring)name, nullptr);
  if (name_str == nullptr)
    return -8;

  *thread_name = name_str;

  java_env->ReleaseStringUTFChars((jstring)name, name_str);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -8;

  return 0;
}

int GetNativeLibraryDirectory(JNIEnv* env, std::string* lib_path) {
  JavaVM* java_vm = g_jvm;

  jclass activity_clazz = env->GetObjectClass(g_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id = env->GetMethodID(activity_clazz, "getNativeLibraryDirectory", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -2;

  jobject name = env->CallObjectMethod(g_obj, method_id);
  if (name == nullptr)
    return -3;

  const char* name_str;
  name_str = env->GetStringUTFChars((jstring)name, nullptr);
  if (name_str == nullptr)
    return -4;

  *lib_path = name_str;

  env->ReleaseStringUTFChars((jstring)name, name_str);

  return 0;
}

int GetCacheLibraryDirectory(JNIEnv *env, std::string* cache_dir) {
  jclass activity_clazz = env->GetObjectClass(g_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id = env->GetMethodID(activity_clazz, "getCacheLibraryDirectory", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -2;

  jobject name = env->CallObjectMethod(g_obj, method_id);
  if (name == nullptr)
    return -3;

  const char* name_str;
  name_str = env->GetStringUTFChars((jstring)name, nullptr);
  if (name_str == nullptr)
    return -4;

  *cache_dir = name_str;

  env->ReleaseStringUTFChars((jstring)name, name_str);

  return 0;
}

int GetDataLibraryDirectory(JNIEnv *env, std::string* data_dir) {
  jclass activity_clazz = env->GetObjectClass(g_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id = env->GetMethodID(activity_clazz, "getDataLibraryDirectory", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -2;

  jobject name = env->CallObjectMethod(g_obj, method_id);
  if (name == nullptr)
    return -3;

  const char* name_str;
  name_str = env->GetStringUTFChars((jstring)name, nullptr);
  if (name_str == nullptr)
    return -4;

  *data_dir = name_str;

  env->ReleaseStringUTFChars((jstring)name, name_str);

  return 0;
}

int GetCurrentLocale(JNIEnv *env, std::string* locale_name) {
  jclass activity_clazz = env->GetObjectClass(g_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id = env->GetMethodID(activity_clazz, "getCurrentLocale", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -2;

  jobject name = env->CallObjectMethod(g_obj, method_id);
  if (name == nullptr)
    return -3;

  const char* name_str;
  name_str = env->GetStringUTFChars((jstring)name, nullptr);
  if (name_str == nullptr)
    return -4;

  *locale_name = name_str;

  env->ReleaseStringUTFChars((jstring)name, name_str);

  return 0;
}

int OpenApkAsset(const std::string& file_path,
                 gurl_base::MemoryMappedFile::Region* region) {
  // The AssetManager API of the NDK does not expose a method for accessing raw
  // resources :(

  JavaVM* java_vm = g_jvm;
  JNIEnv* java_env = g_env;

  jint jni_return = g_env ? JNI_OK : java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = g_env ? JNI_OK : java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -1;

  jclass activity_clazz = java_env->GetObjectClass(g_obj);
  if (activity_clazz == nullptr)
    return -1;

  jstring file_path_obj = java_env->NewStringUTF(file_path.c_str());
  if (file_path_obj == nullptr)
    return -1;

  jmethodID method_id = java_env->GetMethodID(activity_clazz, "openApkAssets", "(Ljava/lang/String;)[J");
  if (method_id == nullptr)
    return -1;

  jlongArray array = (jlongArray)java_env->CallObjectMethod(g_obj, method_id, file_path_obj);
  if (array == nullptr)
    return -1;

  jsize array_num = java_env->GetArrayLength(array);

  CHECK_EQ(3, array_num);

  jlong* results = java_env->GetLongArrayElements(array, nullptr);

  int fd = static_cast<int>(results[0]);
  region->offset = results[1];
  // Not a checked_cast because open() may return -1.
  region->size = static_cast<size_t>(results[2]);

  java_env->ReleaseLongArrayElements(array, results, JNI_ABORT);

  jni_return = g_env ? JNI_OK : java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -1;

  return fd;
}

// called from java thread
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_onNativeCreate(JNIEnv *env, jobject obj) {
  jint jni_return = env->GetJavaVM(&g_jvm);
  CHECK_NE(jni_return, JNI_ERR) << "jvm not found";
  g_obj = obj;
  g_env = env;

  a_open_apk_asset = OpenApkAsset;

  // before any log calls
  std::string cache_path;
  CHECK_EQ(0, GetCacheLibraryDirectory(env, &cache_path));
  a_cache_dir = cache_path;

  std::string exe_path;
  GetExecutablePath(&exe_path);
  SetExecutablePath(exe_path);

  std::string data_path;
  CHECK_EQ(0, GetDataLibraryDirectory(env, &data_path));
  a_data_dir = data_path;

  LOG(INFO) << "exe path: " << exe_path;
  LOG(INFO) << "cache dir: " << cache_path;
  LOG(INFO) << "data dir: " << data_path;

  // possible values: en_US, zh_SG_#Hans, zh_CN_#Hans, zh_HK_#Hant
  std::string locale_name;
  CHECK_EQ(0, GetCurrentLocale(env, &locale_name));
  LOG(INFO) << "current locale: " << locale_name;

  Init(env);

  g_env = nullptr;
}

// called from java thread
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_onNativeDestroy(JNIEnv *env, jobject obj) {
  Shutdown();
  g_jvm = nullptr;
  g_obj = nullptr;
  g_env = nullptr;
}

#endif // __ANDROID__
