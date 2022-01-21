#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")

cd $PWD/..
rm -rf build
mkdir build
pushd build
# TODO use correct build number dynamically
/usr/bin/git ls-files --recurse-submodules | tar caf /home/ko/yass/build/yass-1.0.0.tar.gz --xform='s,^./,yass-1.0.0/,' -T -
cp -fv yass-1.0.0.tar.gz ../yass_1.0.0.orig.tar.gz
popd

debuild -b -uc -us


