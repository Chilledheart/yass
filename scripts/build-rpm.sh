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

RPM_VERSION=$TAG
RPM_SUBVERSION=$SUBTAG

/usr/bin/git ls-files --recurse-submodules | \
  tar caf ${RPM_VERSION}.tar.gz --xform="s,^,yass-${RPM_VERSION}/," -T -

mkdir -p $HOME/rpmbuild/SOURCES
cp -fv ${RPM_VERSION}.tar.gz $HOME/rpmbuild/SOURCES

sed "s|__VERSION__|${TAG}|g" yass.spec.in > yass.spec
sed -i "s|__SUBVERSION__|${SUBTAG}|g" yass.spec
mkdir -p $HOME/rpmbuild/SPECS
cp -fv yass.spec $HOME/rpmbuild/SPECS

pushd $HOME/rpmbuild/SPECS/
rpmbuild -v -bs yass.spec
rpmbuild -v -bb yass.spec
popd

# Rename rpms

# from rpm --querytags
ARCH=$(rpm -q --queryformat '%{ARCH}' gcc)

source /etc/os-release
VERSION_ID=$(sed -E 's/[^0-9]+([0-9]+)(\.[0-9]+)*[^0-9]*$/\1/' /etc/redhat-release)
DISTRO=${ID}-${VERSION_ID}

if [ ${ID} = "rocky" -o ${ID} = "centos" -o ${ID} = "rhel" ]; then
  SUFFIX=el${VERSION_ID}
elif [ ${ID} = "fedora" ]; then
  SUFFIX=fc${VERSION_ID}
fi

# under centos 7, some commands might fail because it doesn't separate debuginfo
# for sub package: https://fedoraproject.org/wiki/Changes/SubpackageAndSourceDebuginfo
mv -f "$HOME/rpmbuild/RPMS/${ARCH}/yass-${RPM_VERSION}-${RPM_SUBVERSION}.${SUFFIX}.${ARCH}.rpm" "yass-${DISTRO}.${SUFFIX}.${ARCH}.${TAG}.rpm"
mv -f "$HOME/rpmbuild/RPMS/${ARCH}/yass-debuginfo-${RPM_VERSION}-${RPM_SUBVERSION}.${SUFFIX}.${ARCH}.rpm" "yass-${DISTRO}-debuginfo.${SUFFIX}.${ARCH}.${TAG}.rpm" || true
mv -f "$HOME/rpmbuild/RPMS/${ARCH}/yass-server-${RPM_VERSION}-${RPM_SUBVERSION}.${SUFFIX}.${ARCH}.rpm" "yass-server-${DISTRO}.${SUFFIX}.${ARCH}.${TAG}.rpm"
mv -f "$HOME/rpmbuild/RPMS/${ARCH}/yass-server-debuginfo-${RPM_VERSION}-${RPM_SUBVERSION}.${SUFFIX}.${ARCH}.rpm" "yass-server-${DISTRO}-debuginfo.${SUFFIX}.${ARCH}.${TAG}.rpm" || true
mv -f "$HOME/rpmbuild/RPMS/${ARCH}/yass-client-${RPM_VERSION}-${RPM_SUBVERSION}.${SUFFIX}.${ARCH}.rpm" "yass-client-${DISTRO}.${SUFFIX}.${ARCH}.${TAG}.rpm"
mv -f "$HOME/rpmbuild/RPMS/${ARCH}/yass-client-debuginfo-${RPM_VERSION}-${RPM_SUBVERSION}.${SUFFIX}.${ARCH}.rpm" "yass-client-${DISTRO}-debuginfo.${SUFFIX}.${ARCH}.${TAG}.rpm" || true

echo "Generated rpms: "
for rpm in *.rpm; do
  echo
  echo $rpm :
  echo "======================================================================"
  rpm -qip $rpm
done
