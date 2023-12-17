// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#ifndef YASS_ANDROID_UTILS
#define YASS_ANDROID_UTILS

#ifdef __ANDROID__

#include <jni.h>
#include <string>

#include <base/files/memory_mapped_file.h>

// returning in host byte order
[[nodiscard]]
int32_t GetIpAddress(JNIEnv *env);
int SetJavaThreadName(const std::string& thread_name);
int GetJavaThreadName(std::string* thread_name);
int GetNativeLibraryDirectory(JNIEnv *env, jobject activity_obj, std::string* result);
int GetCacheLibraryDirectory(JNIEnv *env, jobject activity_obj, std::string* result);
int GetDataLibraryDirectory(JNIEnv *env, jobject activity_obj, std::string* result);
int GetCurrentLocale(JNIEnv *env, jobject activity_obj, std::string* result);
int OpenApkAsset(const std::string& file_path,
                 gurl_base::MemoryMappedFile::Region* region);
#ifdef HAVE_C_ARES
int InitializeCares(JNIEnv *env, jobject activity_obj);
#endif // HAVE_C_ARES

#endif // __ANDROID__

#endif //  YASS_ANDROID_UTILS
