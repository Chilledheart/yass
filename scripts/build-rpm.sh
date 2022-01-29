#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")

cd $PWD/..

# update LAST_CHANGE
if [ -d '.git' ]; then
  LAST_CHANGE_REF=$(/usr/bin/git rev-parse HEAD)
  ABBREV_REF=$(/usr/bin/git rev-parse --abbrev-ref HEAD)
  echo -n "${LAST_CHANGE_REF}-refs/branch-heads/${ABBREV_REF}" > LAST_CHANGE
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
