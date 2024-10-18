#!/bin/bash
set -e
set -x

if [ -z "$(which rustup)" ]; then
  echo "rustup not found"
  exit -1
fi

if [ -z "$ANDROID_SDK_ROOT" ]; then
  echo "ANDROID_SDK_ROOT not defined"
  exit -1
fi

if [ -z "$ANDROID_NDK_VER" ]; then
  echo "ANDROID_NDK_VER not defined"
  exit -1
fi
NDK_ROOT="${ANDROID_SDK_ROOT}/ndk/${ANDROID_NDK_VER}"

echo "Adding rustup toolchain..."

rustup toolchain install 1.82.0
rustup default 1.82.0

echo "Adding rustup toolchain...done"

echo "Adding rustup android target..."

rustup target add aarch64-linux-android armv7-linux-androideabi i686-linux-android x86_64-linux-android

echo "Adding rustup android target...done"

mkdir -p ~/.cargo
HAS_CARGO_ANDROID="$(grep target.aarch64-linux-android ~/.cargo/config.toml || :)"
if [ ! -z "$HAS_CARGO_ANDROID" ]; then
  echo "Skip patching cargo config.toml ..."
  exit 0
fi

echo "Patching cargo config.toml ..."

ARCH=$(uname -s)
case "$ARCH" in
  Linux)
    HOST_OS="linux"
    ;;
  Darwin)
    HOST_OS="darwin"
    ;;
  MINGW*|MSYS*)
    HOST_OS="windows"
    ;;
  *)
    echo "Unsupported OS ${ARCH}"
    exit 1
    ;;
esac

cat >> ~/.cargo/config.toml << EOF

[target.aarch64-linux-android]
ar = "$NDK_ROOT/toolchains/llvm/prebuilt/${HOST_OS}-x86_64/bin/llvm-ar"
linker = "$NDK_ROOT/toolchains/llvm/prebuilt/${HOST_OS}-x86_64/bin/aarch64-linux-android24-clang"

[target.armv7-linux-androideabi]
ar = "$NDK_ROOT/toolchains/llvm/prebuilt/${HOST_OS}-x86_64/bin/llvm-ar"
linker = "$NDK_ROOT/toolchains/llvm/prebuilt/${HOST_OS}-x86_64/bin/armv7a-linux-androideabi24-clang"

[target.i686-linux-android]
ar = "$NDK_ROOT/toolchains/llvm/prebuilt/${HOST_OS}-x86_64/bin/llvm-ar"
linker = "$NDK_ROOT/toolchains/llvm/prebuilt/${HOST_OS}-x86_64/bin/i686-linux-android24-clang"

[target.x86_64-linux-android]
ar = "$NDK_ROOT/toolchains/llvm/prebuilt/${HOST_OS}-x86_64/bin/llvm-ar"
linker = "$NDK_ROOT/toolchains/llvm/prebuilt/${HOST_OS}-x86_64/bin/x86_64-linux-android24-clang"
EOF

echo "Patching cargo config.toml ...done"
