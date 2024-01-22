#!/bin/bash
set -e
set -x

if [ -z "$(which rustup)" ]; then
  echo "rustup not found"
  exit -1
fi

echo "Adding rustup toolchain..."

rustup toolchain install 1.75.0
rustup default 1.75.0

echo "Adding rustup toolchain...done"

echo "Adding rustup ios target..."

rustup target add aarch64-apple-ios aarch64-apple-ios-sim x86_64-apple-ios

echo "Adding rustup ios target...done"
