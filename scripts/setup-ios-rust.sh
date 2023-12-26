#!/bin/bash
set -e
set -x

if [ -z "$(which rustup)" ]; then
  echo "rustup not found"
  exit -1
fi

echo "Adding rustup ios target..."

rustup target add aarch64-apple-ios aarch64-apple-ios-sim x86_64-apple-ios

echo "Adding rustup ios target...done"
