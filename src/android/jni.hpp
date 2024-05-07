// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#ifndef YASS_ANDROID_JNI_HPP
#define YASS_ANDROID_JNI_HPP

#ifdef __ANDROID__

#include <jni.h>

extern JavaVM* g_jvm;
extern jobject g_activity_obj;

extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_YassUtils_getServerHost(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_YassUtils_getServerSNI(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jint JNICALL Java_it_gui_yass_YassUtils_getServerPort(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_YassUtils_getUsername(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_YassUtils_getPassword(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jint JNICALL Java_it_gui_yass_YassUtils_getCipher(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jobjectArray JNICALL Java_it_gui_yass_YassUtils_getCipherStrings(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_YassUtils_getDoHUrl(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_YassUtils_getDoTHost(JNIEnv* env, jobject obj);
extern "C" JNIEXPORT jint JNICALL Java_it_gui_yass_YassUtils_getTimeout(JNIEnv* env, jobject obj);

extern "C" JNIEXPORT jobject JNICALL Java_it_gui_yass_YassUtils_saveConfig(JNIEnv* env,
                                                                           jobject obj,
                                                                           jobject server_host,
                                                                           jobject server_sni,
                                                                           jobject server_port,
                                                                           jobject username,
                                                                           jobject password,
                                                                           jint method_idx,
                                                                           jobject doh_url,
                                                                           jobject dot_host,
                                                                           jobject connect_timeout);

extern "C" JNIEXPORT void JNICALL
Java_it_gui_yass_YassUtils_setEnablePostQuantumKyber(JNIEnv* env, jobject obj, jboolean enable_post_quantum_kyber);

#endif  // __ANDROID__

#endif  // YASS_ANDROID_JNI_HPP
