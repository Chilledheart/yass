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

rustup toolchain install nightly
rustup component add rust-src --toolchain nightly
rustup default nightly
# cargo +nightly run -Z build-std --target aarch64-unknown-linux-ohos
# cargo +nightly run -Z build-std --target x86_64-unknown-linux-ohos

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
