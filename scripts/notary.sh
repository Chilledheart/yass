#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

if [ -z $1 ]; then
  echo "Usage $0 <tag>"
  exit -1
fi

function notarize {
  TAG=$1
  ARCH=$2
  UNSIGNED_DMG=yass-macos-release-$ARCH-$TAG-unsigned.dmg
  SIGNED_DMG=yass-macos-release-$ARCH-$TAG.dmg
  curl -L https://github.com/Chilledheart/yass/releases/download/$TAG/$UNSIGNED_DMG -o $SIGNED_DMG
  xcrun notarytool submit $SIGNED_DMG --keychain-profile notary --wait
  xcrun stapler staple $SIGNED_DMG
  gh release upload $TAG $SIGNED_DMG
  gh release delete-asset -y $TAG $UNSIGNED_DMG
}

function checksum {
  TAG=$1
  ARCH=$2
  SIGNED_DMG=yass-macos-release-$ARCH-$TAG.dmg
  sha256sum $SIGNED_DMG
}

notarize $1 x64
notarize $1 arm64

checksum $1 x64
checksum $1 arm64
