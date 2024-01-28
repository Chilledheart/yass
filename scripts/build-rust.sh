#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

DEFAULT_TARGET=$(rustc -vV | sed -n 's|host: ||p')

curl -L -O -C - https://static.rust-lang.org/dist/rustc-1.75.0-src.tar.gz
curl -L -O -C - https://github.com/rust-lang/cargo/archive/refs/tags/0.76.0.tar.gz

rm -rf cargo-0.76.0
rm -rf rustc-1.75.0-src
tar -xf 0.76.0.tar.gz
tar -xf rustc-1.75.0-src.tar.gz
rm -rf rustc-1.75.0-src/src/tools/cargo
mv cargo-0.76.0 rustc-1.75.0-src/src/tools/cargo

cat > rustc-1.75.0-src/config.toml.in << EOF

profile = "compiler"
change-id = 116881

[build]
# see https://github.com/llvm/llvm-project/issues/60115
sanitizers = false
profiler = false
extended = true
cargo = "$HOME/.cargo/bin/cargo"
rustc = "$HOME/.cargo/bin/rustc"
target = ["$DEFAULT_TARGET", "aarch64-unknown-linux-ohos", "x86_64-unknown-linux-ohos"]
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

[target.x86_64-unknown-linux-ohos]
cc = "$PWD/scripts/x86_64-unknown-linux-ohos-clang.sh"
cxx = "$PWD/scripts/x86_64-unknown-linux-ohos-clang++.sh"
ar = "$HARMONY_NDK_ROOT/native/llvm/bin/llvm-ar"
ranlib = "$HARMONY_NDK_ROOT/native/llvm/bin/llvm-ranlib"
linker = "$PWD/scripts/x86_64-unknown-linux-ohos-clang.sh"
EOF

pushd rustc-1.75.0-src
./configure --prefix $PWD/third_party/rust-ohos --tools=cargo,analysis,rust-demangler,src --enable-local-rust
mv config.toml.in config.toml
make
make install
popd
