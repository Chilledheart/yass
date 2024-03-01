// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifdef __ANDROID__

#include "android/utils.hpp"

#include <arpa/inet.h>

#ifdef HAVE_C_ARES
#include <ares.h>
#endif

#include "android/jni.hpp"
#include "core/logging.hpp"

int32_t GetIpAddress(JNIEnv* env) {
  DCHECK(g_jvm) << "jvm not available";
  DCHECK(g_activity_obj) << "activity not available";

  JavaVM* java_vm = g_jvm;

  jclass activity_clazz = env->GetObjectClass(g_activity_obj);
  if (activity_clazz == nullptr)
    return 0;

  jmethodID method_id = env->GetMethodID(activity_clazz, "getIpAddress", "()I");
  if (method_id == nullptr)
    return 0;

  jint ip_address = env->CallIntMethod(g_activity_obj, method_id);

  return ntohl(ip_address);
}

// Set native thread name inside
int SetJavaThreadName(const std::string& thread_name) {
  DCHECK(g_jvm) << "jvm not available";

  JavaVM* java_vm = g_jvm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass thread_clazz = java_env->FindClass("java/lang/Thread");
  if (thread_clazz == nullptr)
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
  DCHECK(g_jvm) << "jvm not available";

  JavaVM* java_vm = g_jvm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass thread_clazz = java_env->FindClass("java/lang/Thread");
  if (thread_clazz == nullptr)
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

int GetNativeLibraryDirectory(JNIEnv* env, jobject activity_obj, std::string* lib_path) {
  JavaVM* java_vm = g_jvm;

  jclass activity_clazz = env->GetObjectClass(activity_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id = env->GetMethodID(activity_clazz, "getNativeLibraryDirectory", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -2;

  jobject name = env->CallObjectMethod(activity_obj, method_id);
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

int GetCacheLibraryDirectory(JNIEnv* env, jobject activity_obj, std::string* cache_dir) {
  jclass activity_clazz = env->GetObjectClass(activity_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id = env->GetMethodID(activity_clazz, "getCacheLibraryDirectory", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -2;

  jobject name = env->CallObjectMethod(activity_obj, method_id);
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

int GetDataLibraryDirectory(JNIEnv* env, jobject activity_obj, std::string* data_dir) {
  jclass activity_clazz = env->GetObjectClass(activity_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id = env->GetMethodID(activity_clazz, "getDataLibraryDirectory", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -2;

  jobject name = env->CallObjectMethod(activity_obj, method_id);
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

int GetCurrentLocale(JNIEnv* env, jobject activity_obj, std::string* locale_name) {
  jclass activity_clazz = env->GetObjectClass(activity_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id = env->GetMethodID(activity_clazz, "getCurrentLocale", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -2;

  jobject name = env->CallObjectMethod(activity_obj, method_id);
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

int OpenApkAsset(const std::string& file_path, gurl_base::MemoryMappedFile::Region* region) {
  DCHECK(g_jvm) << "jvm not available";
  DCHECK(g_activity_obj) << "activity not available";

  // The AssetManager API of the NDK does not expose a method for accessing raw
  // resources :(

  JavaVM* java_vm = g_jvm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  bool detached = jni_return == JNI_EDETACHED;

  jni_return = detached ? java_vm->AttachCurrentThread(&java_env, nullptr) : JNI_OK;
  if (jni_return != JNI_OK)
    return -1;

  jclass activity_clazz = java_env->GetObjectClass(g_activity_obj);
  if (activity_clazz == nullptr)
    return -1;

  jstring file_path_obj = java_env->NewStringUTF(file_path.c_str());
  if (file_path_obj == nullptr)
    return -1;

  jmethodID method_id = java_env->GetMethodID(activity_clazz, "openApkAssets", "(Ljava/lang/String;)[J");
  if (method_id == nullptr)
    return -1;

  jlongArray array = (jlongArray)java_env->CallObjectMethod(g_activity_obj, method_id, file_path_obj);
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

  jni_return = detached ? java_vm->DetachCurrentThread() : JNI_OK;
  if (jni_return != JNI_OK)
    return -1;

  return fd;
}

#ifdef HAVE_C_ARES
int InitializeCares(JNIEnv* env, jobject activity_obj) {
  DCHECK(g_jvm) << "jvm not available";
  DCHECK(g_activity_obj) << "activity not available";

  JavaVM* java_vm = g_jvm;

  jclass activity_clazz = env->GetObjectClass(g_activity_obj);
  if (activity_clazz == nullptr)
    return -1;

  jmethodID method_id =
      env->GetMethodID(activity_clazz, "getConnectivityManager", "()Landroid/net/ConnectivityManager;");
  if (method_id == nullptr)
    return -2;

  jobject cm = env->CallObjectMethod(g_activity_obj, method_id);
  if (cm == nullptr)
    return -3;

  ares_library_init_jvm(java_vm);
  return ares_library_init_android(cm);
}
#endif  // HAVE_C_ARES

#endif  // __ANDROID__
