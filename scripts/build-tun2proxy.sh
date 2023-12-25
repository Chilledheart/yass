#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

MACHINE=$(uname -m)

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

pushd third_party/tun2proxy
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

popd
