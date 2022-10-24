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
  tar caf 1.0.0.tar.gz --xform='s+^+yass-1.0.0/+' -T -

mkdir -p $HOME/rpmbuild/SOURCES
cp -fv 1.0.0.tar.gz $HOME/rpmbuild/SOURCES

mkdir -p $HOME/rpmbuild/SPECS
cp -fv yass.spec $HOME/rpmbuild/SPECS

pushd $HOME/rpmbuild/SPECS/
rpmbuild -v -bs yass.spec
rpmbuild -v -bb yass.spec
popd

# Rename rpms

# from rpm --querytags
ARCH=$(rpm -q --queryformat '%{ARCH}' glibc)

source /etc/os-release
VERSION_ID=$(sed -E 's/[^0-9]+([0-9]+)(\.[0-9]+)?[^0-9]+$/\1/' /etc/redhat-release)
DISTRO=${ID}-${VERSION_ID}

if [ ${ID} = "rocky" -o ${ID} = "centos" -o ${ID} = "rhel" ]; then
  SUFFIX=el${VERSION_ID}
elif [ ${ID} = "fedora" ]; then
  SUFFIX=fc${VERSION_ID}
fi

# under centos 7, some commands might fail because it doesn't separate debuginfo
# for sub package: https://fedoraproject.org/wiki/Changes/SubpackageAndSourceDebuginfo
mv -f $HOME/rpmbuild/RPMS/${ARCH}/yass-1.0.0-1.${SUFFIX}.${ARCH}.rpm "yass-${DISTRO}.${SUFFIX}.${ARCH}.${TAG}.rpm"
mv -f $HOME/rpmbuild/RPMS/${ARCH}/yass-debuginfo-1.0.0-1.${SUFFIX}.${ARCH}.rpm "yass-${DISTRO}-debuginfo.${SUFFIX}.${ARCH}.${TAG}.rpm" || true
mv -f $HOME/rpmbuild/RPMS/${ARCH}/yass-server-1.0.0-1.${SUFFIX}.${ARCH}.rpm "yass-server-${DISTRO}.${SUFFIX}.${ARCH}.${TAG}.rpm"
mv -f $HOME/rpmbuild/RPMS/${ARCH}/yass-server-debuginfo-1.0.0-1.${SUFFIX}.${ARCH}.rpm "yass-server-${DISTRO}-debuginfo.${SUFFIX}.${ARCH}.${TAG}.rpm" || true
mv -f $HOME/rpmbuild/RPMS/${ARCH}/yass-client-1.0.0-1.${SUFFIX}.${ARCH}.rpm "yass-client-${DISTRO}.${SUFFIX}.${ARCH}.${TAG}.rpm"
mv -f $HOME/rpmbuild/RPMS/${ARCH}/yass-client-debuginfo-1.0.0-1.${SUFFIX}.${ARCH}.rpm "yass-client-${DISTRO}-debuginfo.${SUFFIX}.${ARCH}.${TAG}.rpm" || true

echo "Generated rpms: "
for rpm in *.rpm; do
  echo
  echo $rpm :
  echo "======================================================================"
  rpm -qip $rpm
done
