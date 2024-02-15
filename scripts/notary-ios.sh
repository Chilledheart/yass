#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

if [ -z $1 ]; then
  echo "Usage $0 <tag>"
  exit -1
fi

export DEVELOPER_DIR=/Applications/Xcode-beta.app/Contents/Developer

function notarize {
  TAG=$1
  ARCH=$2
  SIGNED_IPA=yass-ios-release-$ARCH-$TAG.ipa
  xcrun notarytool submit $SIGNED_IPA --keychain-profile notary --wait
  xcrun stapler staple $SIGNED_IPA
  gh release upload $TAG $SIGNED_IPA
}

notarize $1 arm64
