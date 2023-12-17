// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#ifndef _H_ANDROID_YASS_HPP
#define _H_ANDROID_YASS_HPP

#ifdef __ANDROID__

#include <memory>
#include <jni.h>

extern JavaVM                 *g_jvm;
extern JNIEnv                 *g_env;
extern jobject                 g_activity_obj;

extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_onNativeCreate(JNIEnv *env, jobject obj);
extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_onNativeDestroy(JNIEnv *env, jobject obj);

extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_nativeStart(JNIEnv *env, jobject obj);
extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_nativeStop(JNIEnv *env, jobject obj);

extern "C"
JNIEXPORT jlongArray JNICALL Java_it_gui_yass_MainActivity_getRealtimeTransferRate(JNIEnv *env, jobject obj);

#endif // __ANDROID__

#endif //  _H_ANDROID_YASS_HPP
