#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")

cd $PWD/..

# update LAST_CHANGE
if [ -d '.git' ]; then
  LAST_CHANGE_REF=$(/usr/bin/git rev-parse HEAD)
  ABBREV_REF=$(/usr/bin/git rev-parse --abbrev-ref HEAD)
  TAG=$(/usr/bin/git describe --abbrev=40 --tags HEAD || cat TAG)
  echo -n "${LAST_CHANGE_REF}-refs/branch-heads/${ABBREV_REF}" > LAST_CHANGE
  echo -n "${TAG}" > TAG
fi

# TODO use correct build number dynamically
/usr/bin/git ls-files --recurse-submodules | \
  tar caf yass_1.0.0.orig.tar.gz --xform='s,^./,yass-1.0.0/,' -T -

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

debuild -b -uc -us


