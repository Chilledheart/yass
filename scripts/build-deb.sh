#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")

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
  tar caf ../yass_1.0.0.orig.tar.gz --xform='s,^./,yass-1.0.0/,' -T -

# require for a more recent debhelper, like 11.2
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=895044
# Distribution | debhelper | debhelper (backports) |
# Debian 9     | 10.2.5    | 12.1.1
# Debian 10    | 12.1.1    | 13.3.3
# Debian 11    | 13.3.4    | -N/A-
# Ubuntu 16.04 | 10.2.2    | 10.2.2
# Ubuntu 18.04 | 11.1.6    | 13.5.2
# Ubuntu 20.04 | 12.10     | 13.5.2
DEB_VERSION=$(dpkg -s debhelper|grep Version|sed -s 's/^Version: //g')
DEB_HAS_CMAKE_NINIA_BUILD_SUPPORT=$(dpkg --compare-versions $DEB_VERSION ge 11.2 || echo no)
if [ "x$DEB_HAS_CMAKE_NINIA_BUILD_SUPPORT" != "xno" ]; then
  export DEB_BUILD_SYSTEM_OPTIONS="--buildsystem=cmake+ninja"
fi

if [ "x$HOST_ARCH" != "x" ]; then
  if [ "x$SOURCE_ONLY" != "x" ]; then
    dpkg-buildpackage -d --host-arch $HOST_ARCH -S -uc -us
    exit 0
  fi

  export GOPROXY="${GOPROXY:-direct}"
  export CMAKE_CROSS_TOOLCHAIN_FLAGS_NATIVE="-DCROSS_TOOLCHAIN_FLAGS_NATIVE=-DCMAKE_TOOLCHAIN_FILE=$PWD/../Native.cmake"
cat > ../Native.cmake << EOF
set(CMAKE_C_COMPILER ${CC:-gcc})
set(CMAKE_CXX_COMPILER ${CXX:-g++})
EOF
  sbuild --host $HOST_ARCH -d "${HOST_DISTRO}-$(dpkg-architecture -q DEB_BUILD_ARCH)-${HOST_ARCH}" -j $(nproc) --no-apt-update --no-apt-upgrade --no-apt-distupgrade --debbuildopts="-d" --build-dep-resolver=null
else
  dpkg-buildpackage -b -d -uc -us
fi
