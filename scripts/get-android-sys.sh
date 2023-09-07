#!/bin/sh
# Copyright 2019 BSD-3-Clause naiveproxy
set -ex

if [ -z "$1" ]; then
  target_cpu=x64
else
  target_cpu=$1
fi

case "$target_cpu" in
  x64) WITH_ANDROID_IMG=x86_64-24_r08;;
  x86) WITH_ANDROID_IMG=x86-24_r08;;
  arm64) WITH_ANDROID_IMG=arm64-v8a-24_r07;;
  arm) WITH_ANDROID_IMG=armeabi-v7a-24_r07;;
esac

if [ -z "$2" ]; then
  SYSROOT="$PWD/android-sysroot-$target_cpu"
else
  SYSROOT=$2
fi

if [ "$WITH_ANDROID_IMG" -a ! -d out/sysroot-build/android/"$WITH_ANDROID_IMG"/system ]; then
  curl -O https://dl.google.com/android/repository/sys-img/android/$WITH_ANDROID_IMG.zip
  mkdir -p $WITH_ANDROID_IMG/mount
  unzip $WITH_ANDROID_IMG.zip '*/system.img' -d $WITH_ANDROID_IMG
  sudo mount $WITH_ANDROID_IMG/*/system.img $WITH_ANDROID_IMG/mount
  mkdir -p $SYSROOT/system/bin $SYSROOT/system/etc
  cp $WITH_ANDROID_IMG/mount/bin/linker* $SYSROOT/system/bin
  cp $WITH_ANDROID_IMG/mount/etc/hosts $SYSROOT/system/etc
  cp -r $WITH_ANDROID_IMG/mount/lib* $SYSROOT/system
  sudo umount $WITH_ANDROID_IMG/mount
  rm -rf $WITH_ANDROID_IMG $WITH_ANDROID_IMG.zip
fi
