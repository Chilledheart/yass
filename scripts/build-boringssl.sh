#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")

cd $PWD/..

NINJA=$(which ninja-build || which ninja || echo none)

if [ "$NINJA" != "none" ]; then
  GENERATOR=$NINJA
  CMAKE_GENERATOR="-G Ninja"
else
  echo "Ninja not found in PATH"
  GENERATOR=make
  CMAKE_GENERATOR=""
fi

if [ $(uname -s) = "Darwin" ]; then
  NPROC=$(sysctl -n hw.ncpu)
elif [ $(uname -s) = "Linux" ]; then
  NPROC=$(nproc)
else
  NPROC=1
fi

cd third_party/boringssl/src

if [ $(uname -s) = "Darwin" ]; then

  if [ ! -f x64-libssl.a ]; then
    rm -rf build
    mkdir build
    pushd build
    cmake $CMAKE_GENERATOR -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.10 \
      -DCMAKE_OSX_ARCHITECTURES="x86_64" ..
    $GENERATOR -j $NPROC crypto ssl
    cp -fv crypto/libcrypto.a ../x64-libcrypto.a
    cp -fv ssl/libssl.a ../x64-libssl.a
    popd
  fi

  if [ ! -f arm64-libssl.a ]; then
    rm -rf build
    mkdir build
    pushd build
    cmake $CMAKE_GENERATOR -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.10 \
      -DCMAKE_OSX_ARCHITECTURES="arm64" ..
    $GENERATOR -j $NPROC crypto ssl
    cp -fv crypto/libcrypto.a ../arm64-libcrypto.a
    cp -fv ssl/libssl.a ../arm64-libssl.a
    popd
  fi

  if [ ! -f libssl.a ]; then
    lipo -create arm64-libcrypto.a x64-libcrypto.a -output libcrypto.a
    lipo -create arm64-libssl.a x64-libssl.a -output libssl.a
    lipo -info libcrypto.a
    lipo -info libssl.a
  fi

fi

if [ $(uname -s) = "Linux" ]; then
  if [ ! -f libssl.a ]; then
    rm -rf build
    mkdir build
    pushd build
    cmake $CMAKE_GENERATOR -DCMAKE_BUILD_TYPE=Release ..
    $GENERATOR -j $NPROC crypto ssl
    cp -fv crypto/libcrypto.a ../libcrypto.a
    cp -fv ssl/libssl.a ../libssl.a
    popd
  fi
fi

cd ../../..
