#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

ARCH=$(uname -s)
MACHINE=$(uname -m)

WITH_CPU=${WITH_CPU:-${MACHINE}}

LLVM_MINGW_VER=20240518
NASM_VER=2.16.03
NSIS_VER=3.10

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
    if [ ! -d third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-${MACHINE} ]; then
      pushd third_party
      curl -C - -L -O https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VER}/llvm-mingw-${LLVM_MINGW_VER}-ucrt-${MACHINE}.zip
      "/c/Program Files/7-Zip/7z.exe" x llvm-mingw-${LLVM_MINGW_VER}-ucrt-${MACHINE}.zip -aoa
      rm -f llvm-mingw-${LLVM_MINGW_VER}-ucrt-${MACHINE}.zip
      popd
    fi
    LLVM_BASE="$PWD/third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-${MACHINE}"
    ;;
  Linux)
    if [ ! -d third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-20.04-${MACHINE} ]; then
      pushd third_party
      curl -C - -L -O https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VER}/llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-20.04-${MACHINE}.tar.xz
      tar -xf llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-20.04-${MACHINE}.tar.xz
      rm -f llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-20.04-${MACHINE}.tar.xz
      popd
    fi
    LLVM_BASE="$PWD/third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-20.04-${MACHINE}"
    ;;
  Darwin)
    if [ ! -d third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-macos-universal ]; then
      pushd third_party
      curl -C - -L -O https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VER}/llvm-mingw-${LLVM_MINGW_VER}-ucrt-macos-universal.tar.xz
      tar -xf llvm-mingw-${LLVM_MINGW_VER}-ucrt-macos-universal.tar.xz
      rm -f llvm-mingw-${LLVM_MINGW_VER}-ucrt-macos-universal.tar.xz
      popd
    fi
    LLVM_BASE="$PWD/third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-macos-universal"
    ;;
  *)
    echo "Unsupported OS ${ARCH}"
    exit 1
    ;;
esac

case "$ARCH" in
  MINGW*|MSYS*)
    if [ ! -d third_party/nasm ]; then
      pushd third_party
      curl -C - -L -O "https://www.nasm.us/pub/nasm/releasebuilds/${NASM_VER}/win64/nasm-${NASM_VER}-win64.zip"
      "/c/Program Files/7-Zip/7z.exe" x nasm-${NASM_VER}-win64.zip -aoa
      rm -rf nasm
      mv -f nasm-${NASM_VER} nasm
      rm -f nasm-${NASM_VER}-win64.zip
      popd
    fi
    if [ ! -d third_party/nsis ]; then
      pushd third_party
      curl -C - -L -o nsis-${NSIS_VER}.zip "https://sourceforge.net/projects/nsis/files/NSIS%203/${NSIS_VER}/nsis-${NSIS_VER}.zip/download"
      "/c/Program Files/7-Zip/7z.exe" x nsis-${NSIS_VER}.zip -aoa
      rm -rf nsis
      mv -f nsis-${NSIS_VER} nsis
      rm -f nsis-${NSIS_VER}.zip
      popd
    fi
    ;;
  *)
    NASM_EXE="$(which nasm)"
    if [ -z "$NASM_EXE" ]; then
      echo "nasm is required but not found in PATH"
    fi
    NSIS_EXE="$(which makensis)"
    if [ -z "$NSIS_EXE" ]; then
      echo "nsis is required but not found in PATH"
    fi
    ;;
esac

pushd tools
go build
popd

export PATH="${LLVM_BASE}/bin:$PATH"

./tools/build --variant gui --arch ${WITH_CPU} --system mingw \
  -build-test -build-benchmark -mingw-dir ${LLVM_BASE} -clang-path ${LLVM_BASE}
