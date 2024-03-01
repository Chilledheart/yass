// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef YASS_ANDROID_JNI_HPP
#define YASS_ANDROID_JNI_HPP

#include <jni.h>

extern JavaVM* g_jvm;
extern jobject g_activity_obj;

extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getServerHost(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getServerSNI(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getServerPort(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getUsername(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getPassword(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getCipher(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jobjectArray JNICALL Java_it_gui_yass_MainActivity_getCipherStrings(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getTimeout(JNIEnv* env, jobject obj);

extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_saveConfig(JNIEnv* env,
                                                                              jobject obj,
                                                                              jobject server_host,
                                                                              jobject server_sni,
                                                                              jobject server_port,
                                                                              jobject username,
                                                                              jobject password,
                                                                              jint method_idx,
                                                                              jobject connect_timeout);

#endif  // YASS_ANDROID_JNI_HPP
