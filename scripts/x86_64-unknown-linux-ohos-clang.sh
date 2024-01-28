#!/bin/sh
exec $HARMONY_NDK_ROOT/native/llvm/bin/clang++ \
  -target x86_64-linux-ohos \
  --sysroot=$HARMONY_NDK_ROOT/native/sysroot \
  -D__MUSL__ \
  "$@"
