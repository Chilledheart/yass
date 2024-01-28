#!/bin/bash
set -e
set -x
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

if [ -z "$(which rustup)" ]; then
  echo "rustup not found"
  exit -1
fi

if [ -z "$HARMONY_NDK_ROOT" ]; then
  echo "HARMONY_NDK_ROOT not defined"
  exit -1
fi

echo "Adding rustup toolchain..."

if [ ! -f third_party/rust-ohos/bin/rustc ]; then
  echo "Not found rustc under rust-ohos directory"
  exit -1
fi

echo "Adding rustup toolchain...done"

mkdir -p ~/.cargo
HAS_CARGO_HARMONY="$(grep target.aarch64-unknown-linux-ohos ~/.cargo/config || :)"
if [ ! -z "$HAS_CARGO_HARMONY" ]; then
  echo "Skip patching cargo config..."
  exit 0
fi

echo "Patching cargo config..."

cat >> ~/.cargo/config << EOF
[target.aarch64-unknown-linux-ohos]
ar = "$HARMONY_NDK_ROOT/native/llvm/bin/llvm-ar"
linker = "$PWD/scripts/aarch64-unknown-linux-ohos-clang.sh"

[target.x86_64-unknown-linux-ohos]
ar = "$HARMONY_NDK_ROOT/native/llvm/bin/llvm-ar"
linker = "$PWD/scripts/x86_64-unknown-linux-ohos-clang.sh"
EOF

echo "Patching cargo config...done"
