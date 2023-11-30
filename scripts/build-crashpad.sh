#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

ARCH=$(uname -s)
MACHINE=$(uname -m)
PYTHON=$(which python3 2>/dev/null || which python 2>/dev/null)

cd third_party

case "$ARCH" in
  Linux)
    ARCH=Linux
    if [ ! -z depot_tools ]; then
      git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    fi
    export PATH="$PWD/depot_tools:$PATH"
  ;;
  MINGW*|MSYS*)
    ARCH=Windows
    if [ ! -d depot_tools ]; then
      curl -L -O https://storage.googleapis.com/chrome-infra/depot_tools.zip
      curl -L -O https://github.com/ninja-build/ninja/releases/download/v1.11.1/ninja-win.zip
      mkdir depot_tools
      pushd depot_tools
      "/c/Program Files/7-Zip/7z.exe" x ../depot_tools.zip -aoa
      "/c/Program Files/7-Zip/7z.exe" x ../ninja-win.zip -aoa
      rm -f ninja ninja.bat
      popd
      rm -f depot_tools.zip ninja-win.zip
    fi
    export PATH="$PWD/depot_tools:$PATH"
  ;;
esac

flags="$flags"'
use_sysroot=false
treat_warnings_as_errors=false'

if [ "$WITH_CPU" ]; then
  flags="$flags
target_cpu=\"$WITH_CPU\""
fi

if [ "$WITH_OS" ]; then
  flags="$flags
target_os=\"$WITH_OS\""
fi

if [ "$WITH_SYSROOT" ]; then
  flags="$flags
target_sysroot=\"//$WITH_SYSROOT\""
fi

flags="$flags
clang_path=\"$(cygpath -m $PWD)/llvm-build/Release+Asserts\"
extra_cflags=\"/MT\"
extra_cflags_cc=\"-I $(cygpath -m $PWD)/libc++ -I $(cygpath -m $PWD)/libc++/trunk/include -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS -D_LIBCPP_OVERRIDABLE_FUNC_VIS='__attribute__((__visibility__(\\\"default\\\")))'\""

bin_flags="$flags
clang_path=\"$(cygpath -m $PWD)/llvm-build/Release+Asserts\"
extra_cflags=\"/MT\"
extra_cflags_cc=\"\""

out="$PWD/crashpad/crashpad/out/Default"
bin_out="$PWD/crashpad/crashpad/out/Binary"

export DEPOT_TOOLS_WIN_TOOLCHAIN=0

mkdir -p crashpad
cd crashpad
fetch --nohistory crashpad || true
gclient sync

cd crashpad
# patch stage
cp -f ../../../scripts/mini_chromium.BUILD.gn third_party/mini_chromium/mini_chromium/build/config/BUILD.gn
sed -i s/__hlt\(0\)/__builtin_trap\(\)/g third_party/mini_chromium/mini_chromium/base/logging.cc
# build stage
rm -rf out
mkdir -p out/Default
echo "$flags" > out/Default/args.gn
gn gen "$out" --script-executable="$PYTHON" --export-compile-comman
ninja -C "$out" client

mkdir -p out/Binary
echo "$bin_flags" > out/Binary/args.gn
gn gen "$bin_out" --script-executable="$PYTHON" --export-compile-comman
ninja -C "$bin_out" crashpad_handler
