#!/bin/bash
set -e
set -x
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

RUST_VER=$(< RUST_REVISION)

if [ -z "$(which rustup)" ]; then
  echo "rustup not found"
  exit -1
fi

echo "Adding rustup toolchain..."

rustup toolchain install $RUST_VER
rustup default $RUST_VER

echo "Adding rustup toolchain...done"

echo "Adding rustup ios target..."

rustup target add aarch64-apple-ios aarch64-apple-ios-sim x86_64-apple-ios

echo "Adding rustup ios target...done"
