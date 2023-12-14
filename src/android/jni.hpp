// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef YASS_ANDROID_JNI_HPP
#define YASS_ANDROID_JNI_HPP

#include <jni.h>

extern "C"
JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getServerHost(JNIEnv *env, jobject obj);
extern "C"
JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getServerPort(JNIEnv *env, jobject obj);
extern "C"
JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getUsername(JNIEnv *env, jobject obj);
extern "C"
JNIEXPORT jobject JNICALL Java_it_gui_yass_MainActivity_getPassword(JNIEnv *env, jobject obj);
extern "C"
JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getCipher(JNIEnv *env, jobject obj);
extern "C"
JNIEXPORT jobjectArray JNICALL Java_it_gui_yass_MainActivity_getCipherStrings(JNIEnv *env, jobject obj);
extern "C"
JNIEXPORT jint JNICALL Java_it_gui_yass_MainActivity_getTimeout(JNIEnv *env, jobject obj);

extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setServerHost(JNIEnv *env, jobject obj, jobject value);
extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setServerPort(JNIEnv *env, jobject obj, jint value);
extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setUsername(JNIEnv *env, jobject obj, jobject value);
extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setPassword(JNIEnv *env, jobject obj, jobject value);
extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setCipher(JNIEnv *env, jobject obj, jint value);
extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_MainActivity_setTimeout(JNIEnv *env, jobject obj, jint value);


#endif // YASS_ANDROID_JNI_HPP
