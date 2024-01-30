#!/bin/sh
exec $HARMONY_NDK_ROOT/native/llvm/bin/clang \
  -target arm-linux-ohos \
  --sysroot=$HARMONY_NDK_ROOT/native/sysroot \
  -D__MUSL__ \
  -march=armv7-a \
  -mfloat-abi=softfp \
  -mtune=generic-armv7-a \
  -mthumb \
  "$@"
