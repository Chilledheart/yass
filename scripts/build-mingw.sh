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

export PATH="${LLVM_BASE}/bin:$PATH"
rm -rf build-mingw-${WITH_CPU}
mkdir build-mingw-${WITH_CPU}
cd build-mingw-${WITH_CPU}
export CC=clang
export CXX=clang++
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCLI=off -DSERVER=off -DGUI=on .. \
  -DBUILD_TESTS=on -DBUILD_BENCHMARKS=on \
  -DCMAKE_C_COMPILER_TARGET=${WITH_CPU}-w64-mingw32 -DCMAKE_CXX_COMPILER_TARGET=${WITH_CPU}-w64-mingw32 -DCMAKE_ASM_COMPILER_TARGET=${WITH_CPU}-w64-mingw32 \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_SYSTEM_PROCESSOR=${WITH_CPU} \
  -DCMAKE_ASM_FLAGS="--start-no-unused-arguments -rtlib=compiler-rt -fuse-ld=lld --end-no-unused-arguments" \
  -DCMAKE_C_FLAGS="--start-no-unused-arguments -rtlib=compiler-rt -fuse-ld=lld --end-no-unused-arguments" \
  -DCMAKE_CXX_FLAGS="--start-no-unused-arguments -rtlib=compiler-rt -stdlib=libc++ -fuse-ld=lld --end-no-unused-arguments" \
  -DCMAKE_SHARED_LINKER_FLAGS="-rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=lld" \
  -DCMAKE_EXE_LINKER_FLAGS="-rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=lld" \
  -DCMAKE_RC_COMPILER="${LLVM_BASE}/bin/${WITH_CPU}-w64-mingw32-windres" -DCMAKE_RC_COMPILER_ARG1="--codepage 65001"
ninja yass yass_test yass_benchmark

llvm-objcopy --only-keep-debug yass.exe yass.exe.dbg
# stripped executable.
llvm-objcopy --strip-debug yass.exe
# to add a link to the debugging info into the stripped executable.
llvm-objcopy --add-gnu-debuglink=yass.exe.dbg yass.exe
