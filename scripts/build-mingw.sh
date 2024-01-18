#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

ARCH=$(uname -s)
MACHINE=$(uname -m)

WITH_CPU=${WITH_CPU:-${MACHINE}}

case "$WITH_CPU" in
  x86|i586|i686)
    WITH_CPU="i686"
    ;;
  x86_64)
    WITH_CPU="x86_64"
    ;;
  arch64|arm64)
    WITH_CPU="aarch64"
    ;;
esac

case "$ARCH" in
  MINGW*|MSYS*)
    if [ ! -d third_party/llvm-mingw-20231128-ucrt-${MACHINE} ]; then
      pushd third_party
      curl -C - -L -O https://github.com/mstorsjo/llvm-mingw/releases/download/20231128/llvm-mingw-20231128-ucrt-${MACHINE}.zip
      "/c/Program Files/7-Zip/7z.exe" x llvm-mingw-20231128-ucrt-${MACHINE}.zip -aoa
      rm -f llvm-mingw-20231128-ucrt-${MACHINE}.zip
      popd
    fi
    LLVM_BASE="$PWD/third_party/llvm-mingw-20231128-ucrt-${MACHINE}"
    ;;
  Linux)
    if [ ! -d third_party/llvm-mingw-20231128-ucrt-ubuntu-20.04-${MACHINE} ]; then
      pushd third_party
      curl -C - -L -O https://github.com/mstorsjo/llvm-mingw/releases/download/20231128/llvm-mingw-20231128-ucrt-ubuntu-20.04-${MACHINE}.tar.xz
      tar -xf llvm-mingw-20231128-ucrt-ubuntu-20.04-${MACHINE}.tar.xz
      rm -f llvm-mingw-20231128-ucrt-ubuntu-20.04-${MACHINE}.tar.xz
      popd
    fi
    LLVM_BASE="$PWD/third_party/llvm-mingw-20231128-ucrt-ubuntu-20.04-${MACHINE}"
    ;;
  Darwin)
    if [ ! -d third_party/llvm-mingw-20231128-ucrt-macos-universal ]; then
      pushd third_party
      curl -C - -L -O https://github.com/mstorsjo/llvm-mingw/releases/download/20231128/llvm-mingw-20231128-ucrt-macos-universal.tar.xz
      tar -xf llvm-mingw-20231128-ucrt-macos-universal.tar.xz
      rm -f llvm-mingw-20231128-ucrt-macos-universal.tar.xz
      popd
    fi
    LLVM_BASE="$PWD/third_party/llvm-mingw-20231128-ucrt-macos-universal"
    ;;
  *)
    echo "Unsupported OS ${ARCH}"
    exit 1
    ;;
esac

pushd tools
go build
popd

export PATH="${LLVM_BASE}/bin:$PATH"

./tools/build --variant gui --arch ${WITH_CPU} --system mingw \
  -build-test -build-benchmark -mingw-dir ${LLVM_BASE} -clang-path ${LLVM_BASE}
