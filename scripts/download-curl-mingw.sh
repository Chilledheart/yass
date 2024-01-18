#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

# Script for download toolchains (mingw)
#
# Usage: download-toolchain-mingw.sh [arch]

ARCH=$(uname -s)

case "$ARCH" in
  MINGW*|MSYS*)
    P7Z_BIN="/c/Program Files/7-Zip/7z.exe"
    ;;
  *)
    P7Z_BIN=7z
    ;;
esac

cd third_party

function download_curl {
local SUFFIX=$1
rm -rf curl-8.4.0_7-$SUFFIX-mingw
curl -C - -L -O https://curl.se/windows/dl-8.4.0_7/curl-8.4.0_7-$SUFFIX-mingw.zip
"${P7Z_BIN}" x curl-8.4.0_7-$SUFFIX-mingw.zip -aoa
rm -f curl-8.4.0_7-$SUFFIX-mingw.zip
}

if [ -z "$1" ] || [ "$1" = "i686" ]; then
  download_curl win32
elif [ "$1" = "x86_64" ]; then
  download_curl win64
elif [ "$1" = "aarch64" ]; then
  download_curl win64a
else
  echo "Unsupported architecture: $1"
  exit -1
fi
