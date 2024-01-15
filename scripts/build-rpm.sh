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

function build_source() {
/usr/bin/git ls-files --recurse-submodules | \
  tar caf yass-${RPM_VERSION}.tar.gz --xform="s,^,yass-${RPM_VERSION}/," -T -

mkdir -p $HOME/rpmbuild/SOURCES
mv -fv yass-${RPM_VERSION}.tar.gz $HOME/rpmbuild/SOURCES
}

if [ -z "$1" ]; then
  build_source
else
  mkdir -p $HOME/rpmbuild/SOURCES
  cp -v "$1" $HOME/rpmbuild/SOURCES/yass-${RPM_VERSION}.tar.gz
fi

sed "s|__VERSION__|${TAG}|g" yass.spec.in > yass.spec
sed -i "s|__SUBVERSION__|${SUBTAG}|g" yass.spec
mkdir -p $HOME/rpmbuild/SPECS
mv -fv yass.spec $HOME/rpmbuild/SPECS

[ "a$DISABLE_LLD" != "a" ] && rpm_options="--with=disable_lld"

rpm_options="--with=toolchain_clang $rpm_options"
rpm_options="--with=tests_cares $rpm_options"

# from rpm --querytags
ARCH=$(rpm -q --queryformat '%{ARCH}' gcc)

source /etc/os-release
if [ -f /etc/redhat-release ]; then
  VERSION_ID=$(sed -E 's/[^0-9]+([0-9]+)(\.[0-9]+)*[^0-9]*$/\1/' /etc/redhat-release)
fi
DISTRO=${ID}-${VERSION_ID}

if [ ${ID} = "rocky" -o ${ID} = "centos" -o ${ID} = "rhel" ]; then
  SUFFIX=.el${VERSION_ID}
  REAL_SUFFIX=.el${VERSION_ID}
elif [ ${ID} = "fedora" ]; then
  SUFFIX=.fc${VERSION_ID}
  REAL_SUFFIX=.fc${VERSION_ID}
elif [ ${ID} = "opensuse-leap" ]; then
  VERSION_ID=$(echo $VERSION_ID | sed s/\\\.//g)
  SUFFIX=""
  REAL_SUFFIX=.lp${VERSION_ID}
fi

pushd $HOME/rpmbuild/SPECS/
rpmbuild --define "_topdir $HOME/rpmbuild" -v $rpm_options -bs yass.spec
rpmlint "$HOME/rpmbuild/SRPMS/yass-${RPM_VERSION}-${RPM_SUBVERSION}${SUFFIX}.src.rpm"
rpmbuild --define "_topdir $HOME/rpmbuild" -v $rpm_options -bb yass.spec
popd

# Rename rpms

# under centos 7, some commands might fail because it doesn't separate debuginfo
# for sub package: https://fedoraproject.org/wiki/Changes/SubpackageAndSourceDebuginfo
cp -vf "$HOME/rpmbuild/RPMS/${ARCH}/yass-${RPM_VERSION}-${RPM_SUBVERSION}${SUFFIX}.${ARCH}.rpm" "yass${REAL_SUFFIX}.${ARCH}.${RPM_VERSION}-${RPM_SUBVERSION}.rpm"
cp -vf "$HOME/rpmbuild/RPMS/${ARCH}/yass-debuginfo-${RPM_VERSION}-${RPM_SUBVERSION}${SUFFIX}.${ARCH}.rpm" "yass-debuginfo${REAL_SUFFIX}.${ARCH}.${RPM_VERSION}-${RPM_SUBVERSION}.rpm" || true
cp -vf "$HOME/rpmbuild/RPMS/${ARCH}/yass-server-${RPM_VERSION}-${RPM_SUBVERSION}${SUFFIX}.${ARCH}.rpm" "yass-server${REAL_SUFFIX}.${ARCH}.${RPM_VERSION}-${RPM_SUBVERSION}.rpm"
cp -vf "$HOME/rpmbuild/RPMS/${ARCH}/yass-server-debuginfo-${RPM_VERSION}-${RPM_SUBVERSION}${SUFFIX}.${ARCH}.rpm" "yass-server-debuginfo${REAL_SUFFIX}.${ARCH}.${RPM_VERSION}-${RPM_SUBVERSION}.rpm" || true
cp -vf "$HOME/rpmbuild/RPMS/${ARCH}/yass-client-${RPM_VERSION}-${RPM_SUBVERSION}${SUFFIX}.${ARCH}.rpm" "yass-client${REAL_SUFFIX}.${ARCH}.${RPM_VERSION}-${RPM_SUBVERSION}.rpm"
cp -vf "$HOME/rpmbuild/RPMS/${ARCH}/yass-client-debuginfo-${RPM_VERSION}-${RPM_SUBVERSION}${SUFFIX}.${ARCH}.rpm" "yass-client-debuginfo${REAL_SUFFIX}.${ARCH}.${RPM_VERSION}-${RPM_SUBVERSION}.rpm" || true

echo "Generated rpms: "
for rpm in *.rpm; do
  echo
  echo $rpm :
  echo "======================================================================"
  rpm -qip $rpm
  echo "DEP<=================================================================="
  rpm -qRp $rpm
  echo "DEP>=================================================================="
done
