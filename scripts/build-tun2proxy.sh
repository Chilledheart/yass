#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

MACHINE=$(uname -m)

function build_ios {
case "$WITH_CPU" in
  arm64)
    cargo build --target aarch64-apple-ios --release --lib
    ;;
  *)
    echo "Invalid WITH_CPU: $WITH_CPU"
    exit -1
    ;;
esac
}

function build_ios_sim {
case "$WITH_CPU" in
  x64)
    cargo build --target x86_64-apple-ios --release --lib
    ;;
  arm64)
    cargo build --target aarch64-apple-ios-sim --release --lib
    ;;
  *)
    echo "Invalid WITH_CPU: $WITH_CPU"
    exit -1
    ;;
esac
}

function build_android {
case "$WITH_CPU" in
  x86)
    cargo build --target i686-linux-android --release --lib
    ;;
  x64)
    cargo build --target x86_64-linux-android --release --lib
    ;;
  arm)
    cargo build --target armv7-linux-androideabi --release --lib
    ;;
  arm64)
    cargo build --target aarch64-linux-android --release --lib
    ;;
  *)
    echo "Invalid WITH_CPU: $WITH_CPU"
    exit -1
    ;;
esac
}

case "$MACHINE" in
  x86|i586|i686)
    WITH_CPU_DEFAULT="x86"
    ;;
  x86_64)
    WITH_CPU_DEFAULT="x64"
    ;;
  arch64|arm64)
    WITH_CPU_DEFAULT="arm64"
    ;;
esac

WITH_CPU=${WITH_CPU:-${WITH_CPU_DEFAULT}}

WITH_OS_DEFAULT="android"
WITH_OS=${WITH_OS:-${WITH_OS_DEFAULT}}

pushd third_party/tun2proxy

case "$WITH_OS" in
  ios)
    build_ios
    ;;
  ios-sim)
    build_ios_sim
    ;;
  android)
    build_android
    ;;
  **)
    echo "Unsupported OS ${WITH_OS}"
    ;;
esac

popd
