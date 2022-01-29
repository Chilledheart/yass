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
  tar caf yass_1.0.0.orig.tar.gz --xform='s,^./,yass-1.0.0/,' -T -

debuild -b -uc -us


