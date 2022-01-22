#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")

cd $PWD/..

# TODO use correct build number dynamically
/usr/bin/git ls-files --recurse-submodules | \
  tar caf yass_1.0.0.orig.tar.gz --xform='s,^./,yass-1.0.0/,' -T -

debuild -b -uc -us


