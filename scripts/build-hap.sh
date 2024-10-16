#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

# update LAST_CHANGE
if [ -d '.git' ]; then
  LAST_CHANGE_REF=$(/usr/bin/git rev-parse HEAD)
  TAG=$(/usr/bin/git describe --abbrev=0 --tags HEAD)
  SUBTAG=$(/usr/bin/git rev-list $TAG..HEAD --count)
  ABBREV_REF="$TAG{$SUBTAG}"
  echo -n "${LAST_CHANGE_REF}-refs/branch-heads/${ABBREV_REF}" > LAST_CHANGE
  echo -n "${TAG}" > TAG
  echo -n "${SUBTAG}" > SUBTAG
else
  TAG=$(< TAG)
  SUBTAG=$(< SUBTAG)
fi

VERSION=$TAG
SUBVERSION=$SUBTAG
SUBVERSION_SUFFIX="-$SUBTAG"
if [ $SUBVERSION == 0 ]; then
  SUBVERSION_SUFFIX=""
fi

ARCH=${ARCH:-arm64}

case "$ARCH" in
  arm64)
    ABI="arm64-v8a"
    ;;
  arm)
    ABI="armeabi-v7a"
    ;;
  x64)
    ABI="arm64-v8a"
    ;;
  *)
    echo "Invalid ARCH: $ARCH"
    exit -1
    ;;
esac

BUILD_TYPE=${BUILD_TYPE:-release}

case "$BUILD_TYPE" in
  debug)
    HVIGORW_BUILD_TYPE=debug
    BUILD_TYPE=Debug
    ;;
  release)
    HVIGORW_BUILD_TYPE=release
    BUILD_TYPE=Release
    ;;
  *)
    echo "Invalid BUILD_TYPE: $BUILD_TYPE, expected debug or release"
    exit -1
    ;;
esac

function prepare_build() {
pushd tools
go build
popd
}

function cleanup() {
pushd harmony
hvigorw clean --no-daemon
rm -rf entry/libs
popd
}

function strip_binary() {
pushd build-harmony-${ARCH}
rm -f libyass.so
ninja yass
../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --only-keep-debug libyass.so libyass.so.dbg
../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --strip-debug libyass.so
../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --add-gnu-debuglink=libyass.so.dbg libyass.so
popd
}

function build() {
./tools/build --system harmony --arch ${ARCH} -no-packaging -nc
}

function copy_binary() {
pushd harmony
mkdir -p entry/libs/${ABI}
cp -fv ../build-harmony-${ARCH}/libyass.so entry/libs/${ABI}
popd
}

function package_unsign_hap() {
pushd harmony
hvigorw assembleHap --mode module -p product=default -p buildMode=${HVIGORW_BUILD_TYPE} --no-daemon
cp -fv entry/build/default/outputs/default/entry-default-unsigned.hap \
  ../yass-harmony-${HVIGORW_BUILD_TYPE}-${ARCH}-${VERSION}${SUBVERSION_SUFFIX}.hap
popd
}

function package_sign_hap() {
pushd harmony
hvigorw assembleHap --mode module -p product=default -p buildMode=${HVIGORW_BUILD_TYPE} --no-daemon
java -jar "${HARMONY_NDK_ROOT}/toolchains/lib/hap-sign-tool.jar" sign-app \
  -keyAlias "${HARMONY_SIGNING_KEY_ALIAS}" -keyPwd "${HARMONY_SIGNING_KEY_PASSWORD}" \
  -appCertFile "${HARMONY_SIGNING_CERTFILE}" -profileFile "${HARMONY_SIGNING_PROFILE}" \
  -keystoreFile "${HARMONY_SIGNING_STORE_PATH}" -keystorePwd "${HARMONY_SIGNING_STORE_PASSWORD}" \
  -inFile entry/build/default/outputs/default/entry-default-unsigned.hap \
  -outFile ../yass-harmony-${HVIGORW_BUILD_TYPE}-${ARCH}-${VERSION}${SUBVERSION_SUFFIX}.hap \
  -signAlg SHA256withECDSA -mode localSign -signCode 1
popd
}

function package_hap() {
if [ -z "${HARMONY_SIGNING_CERTFILE}" -o -z "${HARMONY_SIGNING_PROFILE}" ]; then
package_unsign_hap
else
package_sign_hap
fi
}

function package_debuginfo() {
pushd build-harmony-${ARCH}
rm -rf yass-harmony-${HVIGORW_BUILD_TYPE}-${ARCH}-${VERSION}${SUBVERSION_SUFFIX}
mkdir yass-harmony-${HVIGORW_BUILD_TYPE}-${ARCH}-${VERSION}${SUBVERSION_SUFFIX}
cp -fv libyass.so.dbg yass-harmony-${HVIGORW_BUILD_TYPE}-${ARCH}-${VERSION}${SUBVERSION_SUFFIX}/
tar cfz ../yass-harmony-${HVIGORW_BUILD_TYPE}-${ARCH}-${VERSION}${SUBVERSION_SUFFIX}-debuginfo.tgz \
  yass-harmony-${HVIGORW_BUILD_TYPE}-${ARCH}-${VERSION}${SUBVERSION_SUFFIX}
rm -rf yass-harmony-${HVIGORW_BUILD_TYPE}-${ARCH}-${VERSION}${SUBVERSION_SUFFIX}
popd
}

function inspect_pkgs() {
7z l yass-harmony-${HVIGORW_BUILD_TYPE}-${ARCH}-${VERSION}${SUBVERSION_SUFFIX}.hap
tar tvf yass-harmony-${HVIGORW_BUILD_TYPE}-${ARCH}-${VERSION}${SUBVERSION_SUFFIX}-debuginfo.tgz
}

cleanup
prepare_build
build
strip_binary
copy_binary
package_hap
package_debuginfo
inspect_pkgs
