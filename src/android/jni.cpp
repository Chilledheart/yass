// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#ifdef __ANDROID__

#include "android/jni.hpp"

#include "cli/cli_worker.hpp"
#include "config/config.hpp"
#include "crypto/crypter_export.hpp"

#include <vector>

JavaVM* g_jvm = nullptr;
jobject g_activity_obj = nullptr;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  g_jvm = vm;
  return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
  g_jvm = nullptr;
}

JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getServerHost(JNIEnv* env, jobject obj) {
  return env->NewStringUTF(absl::GetFlag(FLAGS_server_host).c_str());
}

JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getServerSNI(JNIEnv* env, jobject obj) {
  return env->NewStringUTF(absl::GetFlag(FLAGS_server_sni).c_str());
}

JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getServerPort(JNIEnv* env, jobject obj) {
  return absl::GetFlag(FLAGS_server_port);
}

JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getUsername(JNIEnv* env, jobject obj) {
  return env->NewStringUTF(absl::GetFlag(FLAGS_username).c_str());
}

JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getPassword(JNIEnv* env, jobject obj) {
  return env->NewStringUTF(absl::GetFlag(FLAGS_password).c_str());
}

JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getCipher(JNIEnv* env, jobject obj) {
  static constexpr const uint32_t method_ids[] = {
#define XX(num, name, string) num,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };
  int method_idx = [&](uint32_t method) -> int {
    auto it = std::find(std::begin(method_ids), std::end(method_ids), method);
    if (it == std::end(method_ids)) {
      return 0;
    }
    return it - std::begin(method_ids);
  }(absl::GetFlag(FLAGS_method).method);
  return method_idx;
}

JNIEXPORT jobjectArray JNICALL Java_it_gui_yass_MainActivity_getCipherStrings(JNIEnv* env, jobject obj) {
  static constexpr const char* const method_names[] = {
#define XX(num, name, string) string,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };
  jobjectArray jarray =
      env->NewObjectArray(std::size(method_names), env->FindClass("java/lang/String"), env->NewStringUTF(""));
  for (unsigned int i = 0; i < std::size(method_names); ++i) {
    env->SetObjectArrayElement(jarray, i, env->NewStringUTF(method_names[i]));
  }
  return jarray;
}

JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getDoHUrl(JNIEnv* env, jobject obj) {
  return env->NewStringUTF(absl::GetFlag(FLAGS_doh_url).c_str());
}

JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getDoTHost(JNIEnv* env, jobject obj) {
  return env->NewStringUTF(absl::GetFlag(FLAGS_dot_host).c_str());
}

JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getTimeout(JNIEnv* env, jobject obj) {
  return absl::GetFlag(FLAGS_connect_timeout);
}

JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_saveConfig(JNIEnv* env,
                                                                   jobject obj,
                                                                   jobject _server_host,
                                                                   jobject _server_sni,
                                                                   jobject _server_port,
                                                                   jobject _username,
                                                                   jobject _password,
                                                                   jint _method_idx,
                                                                   jobject _doh_url,
                                                                   jobject _dot_host,
                                                                   jobject _timeout) {
  const char* server_host_str = env->GetStringUTFChars((jstring)_server_host, nullptr);
  std::string server_host = server_host_str != nullptr ? server_host_str : std::string();
  env->ReleaseStringUTFChars((jstring)_server_host, server_host_str);

  const char* server_sni_str = env->GetStringUTFChars((jstring)_server_sni, nullptr);
  std::string server_sni = server_sni_str != nullptr ? server_sni_str : std::string();
  env->ReleaseStringUTFChars((jstring)_server_sni, server_sni_str);

  const char* server_port_str = env->GetStringUTFChars((jstring)_server_port, nullptr);
  std::string server_port = server_port_str != nullptr ? server_port_str : std::string();
  env->ReleaseStringUTFChars((jstring)_server_port, server_port_str);

  const char* username_str = env->GetStringUTFChars((jstring)_username, nullptr);
  std::string username = username_str != nullptr ? username_str : std::string();
  env->ReleaseStringUTFChars((jstring)_username, username_str);

  const char* password_str = env->GetStringUTFChars((jstring)_password, nullptr);
  std::string password = password_str != nullptr ? password_str : std::string();
  env->ReleaseStringUTFChars((jstring)_password, password_str);

  static std::vector<cipher_method> methods_idxes = {
#define XX(num, name, string) CRYPTO_##name,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };
  DCHECK_LT((uint32_t)_method_idx, methods_idxes.size());
  auto method = methods_idxes[_method_idx];

  std::string local_host = "0.0.0.0";
  std::string local_port = "0";

  const char* doh_url_str = env->GetStringUTFChars((jstring)_doh_url, nullptr);
  std::string doh_url = doh_url_str != nullptr ? doh_url_str : std::string();
  env->ReleaseStringUTFChars((jstring)_doh_url, doh_url_str);

  const char* dot_host_str = env->GetStringUTFChars((jstring)_dot_host, nullptr);
  std::string dot_host = dot_host_str != nullptr ? dot_host_str : std::string();
  env->ReleaseStringUTFChars((jstring)_dot_host, dot_host_str);

  const char* timeout_str = env->GetStringUTFChars((jstring)_timeout, nullptr);
  std::string timeout = timeout_str != nullptr ? timeout_str : std::string();
  env->ReleaseStringUTFChars((jstring)_timeout, timeout_str);

  std::string err_msg = config::ReadConfigFromArgument(server_host, server_sni, server_port, username, password, method,
                                                       local_host, local_port, doh_url, dot_host, timeout);

  if (err_msg.empty()) {
    return nullptr;
  }
  return env->NewStringUTF(err_msg.c_str());
}

#endif  // __ANDROID__
