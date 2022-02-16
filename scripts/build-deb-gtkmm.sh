#!/usr/bin/env bash
set -e
set -x

export HOST_ARCH=${HOST_ARCH:-i386}
export HOST_DISTRO=${HOST_DISTRO:-focal}

# required by debhelper
sudo apt-get install -y gnome-pkg-tools

# download and install source
apt-get source gtkmm3.0
sudo schroot --chroot "source:$HOST_DISTRO-$(dpkg-architecture -q DEB_BUILD_ARCH)-$HOST_ARCH" --user root -- \
  apt-get -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" build-dep -y --host-architecture=$HOST_ARCH gtkmm3.0

# build gtkmm3.0
cd gtkmm3.0-*/
sudo -E -u "$USER" -g sbuild sbuild --host $HOST_ARCH -d "$HOST_DISTRO-$(dpkg-architecture -q DEB_BUILD_ARCH)-$HOST_ARCH" -j $(nproc) --no-apt-update --no-apt-upgrade --no-apt-distupgrade --debbuildopts="-d" --build-dep-resolver=null
cd ../
