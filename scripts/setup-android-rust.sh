#!/bin/bash
set -e
set -x

if [ -z "$(which rustup)" ]; then
  echo "rustup not found"
  exit -1
fi

if [ -z "$ANDROID_NDK_ROOT" ]; then
  echo "ANDROID_NDK_ROOT not defined"
  exit -1
fi

echo "Adding rustup android target..."

rustup target add aarch64-linux-android armv7-linux-androideabi i686-linux-android x86_64-linux-android

echo "Adding rustup android target...done"

mkdir -p ~/.cargo
HAS_CARGO_ANDROID="$(grep target.aarch64-linux-android ~/.cargo/config || :)"
if [ ! -z "$HAS_CARGO_ANDROID" ]; then
  echo "Skip patching cargo config..."
  exit 0
fi

echo "Patching cargo config..."

cat >> ~/.cargo/config << EOF

[target.aarch64-linux-android]
ar = "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
linker = "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"

[target.armv7-linux-androideabi]
ar = "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
linker = "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi24-clang"

[target.i686-linux-android]
ar = "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
linker = "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/i686-linux-android24-clang"

[target.x86_64-linux-android]
ar = "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
linker = "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android24-clang"
EOF

echo "Patching cargo config...done"