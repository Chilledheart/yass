#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
VERSION=1.2.2
SUBVERSION=1

cd $PWD/..

# update LAST_CHANGE
if [ -d '.git' ]; then
  LAST_CHANGE_REF=$(/usr/bin/git rev-parse HEAD)
  LAST_TAG=$(/usr/bin/git describe --abbrev=0 --tags HEAD || echo none)
  if [ "$LAST_TAG" != "none" ]; then
    COUNT_FROM_LATEST_TAG=$(/usr/bin/git rev-list $LATEST_TAG..HEAD --count || cat TAG)
    ABBREV_REF="$LAST_TAG{$COUNT_FROM_LATEST_TAG}"
  else
    ABBREV_REF=$(/usr/bin/git rev-parse --abbrev-ref HEAD)
  fi
  TAG=$(/usr/bin/git describe --abbrev=40 --tags HEAD || cat TAG)
  echo -n "${LAST_CHANGE_REF}-refs/branch-heads/${ABBREV_REF}" > LAST_CHANGE
  echo -n "${TAG}" > TAG
fi

# TODO use correct build number dynamically
/usr/bin/git ls-files --recurse-submodules | \
  tar caf ../yass_${VERSION}.orig.tar.gz --xform="s,^,yass-${VERSION}/," -T -

# require for a more recent debhelper, like 11.2
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=895044
# Distribution | debhelper | debhelper (backports) |
# Debian 9     | 10.2.5    | 12.1.1
# Debian 10    | 12.1.1    | 13.3.3
# Debian 11    | 13.3.4    | -N/A-
# Ubuntu 16.04 | 10.2.2    | 10.2.2
# Ubuntu 18.04 | 11.1.6    | 13.5.2
# Ubuntu 20.04 | 12.10     | 13.5.2
BUILD_ARCH=${BUILD_ARCH:-$(dpkg-architecture -qDEB_BUILD_ARCH)}
BUILD_GNU_TYPE=${BUILD_GNU_TYPE:-$(dpkg-architecture -qDEB_HOST_GNU_TYPE -a$BUILD_ARCH)}
# required by clang because it is amd64 execuable and running under i386 sysroot
if [ "x$CC" != "x" -a "x$($CC --version | grep clang)" != "x" ]; then
  NATIVE_CFLAGS="-target $BUILD_GNU_TYPE -ccc-gcc-name ${BUILD_GNU_TYPE}-gcc"
  NATIVE_CXXFLAGS="-target $BUILD_GNU_TYPE -ccc-gcc-name ${BUILD_GNU_TYPE}-g++"
fi

if [ "x$HOST_ARCH" != "x" ]; then
  DEB_VERSION=$(sudo schroot --chroot "source:$HOST_DISTRO-$BUILD_ARCH-$HOST_ARCH" --user root -- dpkg -s debhelper|grep Version|sed -s 's/^Version: //g')
else
  DEB_VERSION=$(dpkg -s debhelper|grep Version|sed -s 's/^Version: //g')
fi
DEB_HAS_CMAKE_NINIA_BUILD_SUPPORT=$(dpkg --compare-versions $DEB_VERSION ge 11.2 || echo no)

if [ "x$DEB_HAS_CMAKE_NINIA_BUILD_SUPPORT" != "xno" ]; then
  export DEB_BUILD_SYSTEM_OPTIONS="--buildsystem=cmake+ninja"
fi

if [ "x$HOST_ARCH" != "x" ]; then
  if [ "x$SOURCE_ONLY" != "x" ]; then
    dpkg-buildpackage -d --host-arch $HOST_ARCH --source-option="-i.*" -S -uc -us
    exit 0
  fi

  export GOPROXY="${GOPROXY:-direct}"
  export CMAKE_CROSS_TOOLCHAIN_FLAGS_NATIVE="-DCROSS_TOOLCHAIN_FLAGS_NATIVE=-DCMAKE_TOOLCHAIN_FILE=$PWD/../Native.cmake"
cat > ../Native.cmake << EOF
set(CMAKE_C_COMPILER "${CC:-gcc}")
set(CMAKE_CXX_COMPILER "${CXX:-g++}")
set(CMAKE_C_FLAGS "${CFLAGS} ${NATIVE_CFLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CXXFLAGS} ${NATIVE_CXXFLAGS} -fPIC")
EOF
  sbuild --build $BUILD_ARCH --host $HOST_ARCH \
    --dist "${HOST_DISTRO}-$BUILD_ARCH-${HOST_ARCH}" -j $(nproc) \
    --no-apt-update --no-apt-upgrade --no-apt-distupgrade \
    --no-run-lintian --no-run-piuparts --no-run-autopkgtest \
    --nolog --build-dep-resolver=null \
    --dpkg-source-opts="-i.*" \
    --debbuildopts="-d -a$HOST_ARCH"
else
  dpkg-buildpackage -b -d -uc -us
fi

# Rename debs
ARCH=${HOST_ARCH:-$BUILD_ARCH}
DISTRO=$(scripts/get-debian-name.py $HOST_DISTRO)

if [ -f ../"yass_${VERSION}-${SUBVERSION}_${ARCH}.deb" ]; then
  mv -f ../"yass_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass-${DISTRO}_${ARCH}.${TAG}.deb"
  mv -f ../"yass-dbg_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass-${DISTRO}-dbg_${ARCH}.${TAG}.deb"
fi

mv -f ../"yass-server_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass-server-${DISTRO}_${ARCH}.${TAG}.deb"
mv -f ../"yass-server-dbg_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass-server-${DISTRO}-dbg_${ARCH}.${TAG}.deb"
mv -f ../"yass-client_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass-client-${DISTRO}_${ARCH}.${TAG}.deb"
mv -f ../"yass-client-dbg_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass-client-${DISTRO}-dbg_${ARCH}.${TAG}.deb"

echo "Generated debs: "
for deb in *.deb; do
  echo
  echo $deb :
  echo "======================================================================"
  dpkg -I $deb
done
