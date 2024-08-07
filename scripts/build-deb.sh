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
# FIXME deb cannot pickup subtag correctly
SUBVERSION=1

rm -f ../yass_${VERSION}.orig.tar.gz
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
  DPKG_VERSION=$(sudo schroot --chroot "source:$HOST_DISTRO-$BUILD_ARCH-$HOST_ARCH" --user root -- dpkg -s dpkg|grep Version|sed -s 's/^Version: //g')
else
  DEB_VERSION=$(dpkg -s debhelper|grep Version|sed -s 's/^Version: //g')
  DPKG_VERSION=$(dpkg -s dpkg|grep Version|sed -s 's/^Version: //g')
fi
DEB_HAS_CMAKE_NINIA_BUILD_SUPPORT=$(dpkg --compare-versions $DEB_VERSION ge 11.2 || echo no)
DPKG_HAS_LTO_DEFAULT_SUPPORT=$(dpkg --compare-versions $DPKG_VERSION ge 1.21 || echo no)

if [ "x$DEB_HAS_CMAKE_NINIA_BUILD_SUPPORT" != "xno" ]; then
  export DEB_BUILD_SYSTEM_OPTIONS="--buildsystem=cmake+ninja"
fi

# we're using clang, not gcc
# https://wiki.debian.org/ToolChain/LTO
if [ "x$DPKG_HAS_LTO_DEFAULT_SUPPORT" != "xno" ]; then
  export DEB_BUILD_MAINT_OPTIONS=optimize=-lto
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

USE_QT6=$(echo $DEB_BUILD_PROFILES | grep qt6 || :)
USE_QT5=$(echo $DEB_BUILD_PROFILES | grep qt5 || :)
USE_GTK4=$(echo $DEB_BUILD_PROFILES | grep gtk4 || :)
USE_GTK3=$(echo $DEB_BUILD_PROFILES | grep gtk3 || :)

if [ ! -z "$USE_QT6" ]; then
  GUI_SUFFIX=-qt6
elif [ ! -z "$USE_QT5" ]; then
  GUI_SUFFIX=-qt5
elif [ ! -z "$USE_GTK4" ]; then
  GUI_SUFFIX=-gtk4
elif [ ! -z "$USE_GTK3" ]; then
  GUI_SUFFIX=-gtk3
fi

if [ -f ../"yass_${VERSION}-${SUBVERSION}_${ARCH}.deb" ]; then
  mv -f ../"yass_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass${GUI_SUFFIX}-${DISTRO}_${ARCH}.${TAG}${SUBVERSION_SUFFIX}.deb"
  mv -f ../"yass-dbg_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass${GUI_SUFFIX}-${DISTRO}-dbg_${ARCH}.${TAG}${SUBVERSION_SUFFIX}.deb"
fi

mv -f ../"yass-server_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass-server-${DISTRO}_${ARCH}.${TAG}${SUBVERSION_SUFFIX}.deb"
mv -f ../"yass-server-dbg_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass-server-${DISTRO}-dbg_${ARCH}.${TAG}${SUBVERSION_SUFFIX}.deb"
mv -f ../"yass-client_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass-client-${DISTRO}_${ARCH}.${TAG}${SUBVERSION_SUFFIX}.deb"
mv -f ../"yass-client-dbg_${VERSION}-${SUBVERSION}_${ARCH}.deb" "yass-client-${DISTRO}-dbg_${ARCH}.${TAG}${SUBVERSION_SUFFIX}.deb"

echo "Generated debs: "
for deb in *.deb; do
  echo
  echo $deb :
  echo "======================================================================"
  dpkg -I $deb
  dpkg -c $deb
done

echo "md5sum "
echo "======================================================================"
md5sum *.deb

echo "sha1sum "
echo "======================================================================"
sha1sum *.deb

echo "sha256sum "
echo "======================================================================"
sha256sum *.deb

echo "sha512sum "
echo "======================================================================"
sha512sum *.deb
