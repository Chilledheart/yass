#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

DEFAULT_TARGET=$(rustc -vV | sed -n 's|host: ||p')

RUST_VER=1.76.0
CARGO_VER=0.77.0

# https://github.com/Homebrew/homebrew-core/blob/master/Formula/r/rust.rb
curl -L -O -C - https://static.rust-lang.org/dist/rustc-$RUST_VER-src.tar.xz
curl -L -O https://github.com/rust-lang/cargo/archive/refs/tags/$CARGO_VER.tar.gz

rm -rf cargo-$CARGO_VER
rm -rf rustc-$RUST_VER-src
tar -xf $CARGO_VER.tar.gz
tar -xf rustc-$RUST_VER-src.tar.xz
rm -rf rustc-$RUST_VER-src/src/tools/cargo
mv cargo-$CARGO_VER rustc-$RUST_VER-src/src/tools/cargo

cat > rustc-$RUST_VER-src/config.toml.in << EOF

profile = "compiler"
# latest change id in src/bootstrap/src/utils/change_tracker.rs
change-id = 118703

[build]
# see https://github.com/llvm/llvm-project/issues/60115
sanitizers = false
profiler = false
extended = true
cargo = "$HOME/.cargo/bin/cargo"
rustc = "$HOME/.cargo/bin/rustc"
target = ["$DEFAULT_TARGET", "aarch64-unknown-linux-ohos", "armv7-unknown-linux-ohos", "x86_64-unknown-linux-ohos"]
docs = false

[install]
prefix = "$PWD/third_party/rust-ohos"
sysconfdir = "$PWD/third_party/rust-ohos/etc"

[target.aarch64-unknown-linux-ohos]
cc = "$PWD/scripts/aarch64-unknown-linux-ohos-clang.sh"
cxx = "$PWD/scripts/aarch64-unknown-linux-ohos-clang++.sh"
ar = "$HARMONY_NDK_ROOT/native/llvm/bin/llvm-ar"
ranlib = "$HARMONY_NDK_ROOT/native/llvm/bin/llvm-ranlib"
linker = "$PWD/scripts/aarch64-unknown-linux-ohos-clang.sh"

[target.armv7-unknown-linux-ohos]
cc = "$PWD/scripts/armv7-unknown-linux-ohos-clang.sh"
cxx = "$PWD/scripts/armv7-unknown-linux-ohos-clang++.sh"
ar = "$HARMONY_NDK_ROOT/native/llvm/bin/llvm-ar"
ranlib = "$HARMONY_NDK_ROOT/native/llvm/bin/llvm-ranlib"
linker = "$PWD/scripts/armv7-unknown-linux-ohos-clang.sh"

[target.x86_64-unknown-linux-ohos]
cc = "$PWD/scripts/x86_64-unknown-linux-ohos-clang.sh"
cxx = "$PWD/scripts/x86_64-unknown-linux-ohos-clang++.sh"
ar = "$HARMONY_NDK_ROOT/native/llvm/bin/llvm-ar"
ranlib = "$HARMONY_NDK_ROOT/native/llvm/bin/llvm-ranlib"
linker = "$PWD/scripts/x86_64-unknown-linux-ohos-clang.sh"
EOF

pushd rustc-$RUST_VER-src
./configure --prefix $PWD/third_party/rust-ohos --tools=cargo,analysis,rust-demangler,src --enable-local-rust
mv config.toml.in config.toml
make
make install
popd
