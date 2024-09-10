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
# FIXME deb cannot pickup subtag correctly
SUBVERSION=1

TARBALLS="yass-${VERSION}.tar.gz yass-${VERSION}.tar.bz2 yass-${VERSION}.tar.xz yass-${VERSION}.tar.zst yass-${VERSION}.zip"

rm -f yass-${VERSION}.tar ${TARBALLS}

/usr/bin/git ls-files --recurse-submodules | \
  tar caf yass-${VERSION}.tar --xform="s,^,yass-${VERSION}/," -T -

gzip -9 --keep yass-${VERSION}.tar
bzip2 -9 --keep yass-${VERSION}.tar
xz -T0 -9 -f --keep yass-${VERSION}.tar
zstd -T0 -19 -f --keep yass-${VERSION}.tar

rm -f yass-${VERSION}.tar

# doesn't work because zip cannot create prefix
# /usr/bin/git ls-files --recurse-submodules | \
#   zip -@ yass-${VERSION}.zip

/usr/bin/git ls-files --recurse-submodules | \
  bsdtar caf yass-${VERSION}.zip -s ",^,yass-${VERSION}/," -T -

echo "md5sum "
echo "======================================================================"
md5sum ${TARBALLS}

echo "sha1sum "
echo "======================================================================"
sha1sum ${TARBALLS}

echo "sha256sum "
echo "======================================================================"
sha256sum ${TARBALLS}

echo "sha512sum "
echo "======================================================================"
sha512sum ${TARBALLS}
