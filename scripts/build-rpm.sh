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
rpmbuild -bs yass.spec
rpmbuild -bb yass.spec
popd

# Rename rpms

DISTRO=${DISTRO:-centos8}

mv -f $HOME/rpmbuild/RPMS/x86_64/yass-1.0.0-1.el8.x86_64.rpm "yass-${DISTRO}.el8.x86_64.${TAG}.rpm"
mv -f $HOME/rpmbuild/RPMS/x86_64/yass-debuginfo-1.0.0-1.el8.x86_64.rpm "yass-${DISTRO}-debuginfo.el8.x86_64.${TAG}.rpm"
mv -f $HOME/rpmbuild/RPMS/x86_64/yass-server-1.0.0-1.el8.x86_64.rpm "yass-server-${DISTRO}.el8.x86_64.${TAG}.rpm"
mv -f $HOME/rpmbuild/RPMS/x86_64/yass-server-debuginfo-1.0.0-1.el8.x86_64.rpm "yass-server-${DISTRO}-debuginfo.el8.x86_64.${TAG}.rpm"
mv -f $HOME/rpmbuild/RPMS/x86_64/yass-client-1.0.0-1.el8.x86_64.rpm "yass-client-${DISTRO}.el8.x86_64.${TAG}.rpm"
mv -f $HOME/rpmbuild/RPMS/x86_64/yass-client-debuginfo-1.0.0-1.el8.x86_64.rpm "yass-client-${DISTRO}-debuginfo.el8.x86_64.${TAG}.rpm"

echo "Generated rpms: "
for rpm in *.rpm; do
  echo
  echo $rpm :
  echo "======================================================================"
  rpm -qi $rpm
done
