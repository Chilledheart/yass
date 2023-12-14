// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifdef __ANDROID__

#include "android/jni.hpp"

#include "config/config.hpp"
#include "crypto/crypter_export.hpp"

JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getServerHost(JNIEnv *env, jobject obj) {
  return env->NewStringUTF(absl::GetFlag(FLAGS_server_host).c_str());
}

JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getServerPort(JNIEnv *env, jobject obj) {
  return absl::GetFlag(FLAGS_server_port);
}

JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getUsername(JNIEnv *env, jobject obj) {
  return env->NewStringUTF(absl::GetFlag(FLAGS_username).c_str());
}

JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getPassword(JNIEnv *env, jobject obj) {
  return env->NewStringUTF(absl::GetFlag(FLAGS_password).c_str());
}

JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getCipher(JNIEnv *env, jobject obj) {
  std::vector<cipher_method> methods_idxes = {
#define XX(num, name, string) CRYPTO_##name,
CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };
  int method_idx = [&](cipher_method method) -> int {
    auto it = std::find(methods_idxes.begin(), methods_idxes.end(), method);
    if (it == methods_idxes.end()) {
      return 0;
    }
    return it - methods_idxes.begin();
  }(absl::GetFlag(FLAGS_method).method);
  return method_idx;
}

JNIEXPORT jobjectArray JNICALL Java_it_gui_yass_MainActivity_getCipherStrings(JNIEnv *env, jobject obj) {
  std::vector<const char*> methods = {
#define XX(num, name, string) CRYPTO_##name##_STR,
CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };
  jobjectArray jarray = env->NewObjectArray(methods.size(),
                                            env->FindClass("java/lang/String"),
                                            env->NewStringUTF(""));
  for (unsigned int i = 0; i < methods.size(); ++i) {
    env->SetObjectArrayElement(jarray, i, env->NewStringUTF(methods[i]));
  }
  return jarray;
}

JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getTimeout(JNIEnv *env, jobject obj) {
  return absl::GetFlag(FLAGS_connect_timeout);
}

JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setServerHost(JNIEnv *env, jobject obj, jobject value) {
  const char* value_str = env->GetStringUTFChars((jstring)value, nullptr);
  absl::SetFlag(&FLAGS_server_host, value_str);
  env->ReleaseStringUTFChars((jstring)value, value_str);
}

JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setServerPort(JNIEnv *env, jobject obj, jint value) {
  absl::SetFlag(&FLAGS_server_port, value);
}

JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setUsername(JNIEnv *env, jobject obj, jobject value) {
  const char* value_str = env->GetStringUTFChars((jstring)value, nullptr);
  absl::SetFlag(&FLAGS_username, value_str);
  env->ReleaseStringUTFChars((jstring)value, value_str);
}

JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setPassword(JNIEnv *env, jobject obj, jobject value) {
  const char* value_str = env->GetStringUTFChars((jstring)value, nullptr);
  absl::SetFlag(&FLAGS_password, value_str);
  env->ReleaseStringUTFChars((jstring)value, value_str);
}

JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setCipher(JNIEnv *env, jobject obj, jint value) {
  std::vector<cipher_method> methods_idxes = {
#define XX(num, name, string) CRYPTO_##name,
CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };
  DCHECK_LT((uint32_t)value, methods_idxes.size());
  absl::SetFlag(&FLAGS_method, methods_idxes[value]);
}

JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setTimeout(JNIEnv *env, jobject obj, jint value) {
  absl::SetFlag(&FLAGS_connect_timeout, value);
}

#endif // __ANDROID__
