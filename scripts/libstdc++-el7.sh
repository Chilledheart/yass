#!/bin/sh
set -x
set -e

# to make chromium's clang binrary compatible with centos 7 / rhel 7
# upgrade libstdc++.so.6 from centos7's devtoolset should be sufficient

PWD=$(dirname "${BASH_SOURCE[0]}")

cd $PWD/..

cp -f scripts/libstdc++-symbols.ver /tmp

pushd /tmp

curl -L -O http://mirror.centos.org/centos/7/sclo/x86_64/rh/Packages/d/devtoolset-11-libstdc++-devel-11.2.1-1.2.el7.x86_64.rpm

rpm2cpio devtoolset-11-libstdc++-devel-11.2.1-1.2.el7.x86_64.rpm | cpio -idvm

# Full command line:
# /builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/./gcc/xgcc -shared-libgcc -B/builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/./gcc -nostdinc++ -L/builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/x86_64-redhat-linux/libstdc++-v3/src -L/builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/x86_64-redhat-linux/libstdc++-v3/src/.libs -L/builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/x86_64-redhat-linux/libstdc++-v3/libsupc++/.libs -B/opt/rh/devtoolset-11/root/usr/x86_64-redhat-linux/bin/ -B/opt/rh/devtoolset-11/root/usr/x86_64-redhat-linux/lib/ -isystem /opt/rh/devtoolset-11/root/usr/x86_64-redhat-linux/include -isystem /opt/rh/devtoolset-11/root/usr/x86_64-redhat-linux/sys-include   -fno-checking  -fPIC -DPIC -D_GLIBCXX_SHARED -shared -nostdlib /lib/../lib64/crti.o /builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/./gcc/crtbeginS.o  .libs/compatibility.o .libs/compatibility-debug_list.o .libs/compatibility-debug_list-2.o .libs/compatibility-c++0x.o .libs/compatibility-atomic-c++0x.o .libs/compatibility-thread-c++0x.o .libs/compatibility-chrono.o .libs/compatibility-condvar.o  -Wl,--whole-archive ../libsupc++/.libs/libsupc++convenience.a ../src/c++98/.libs/libc++98convenience.a ../src/c++11/.libs/libc++11convenience.a ../src/c++17/.libs/libc++17convenience.a ../src/c++20/.libs/libc++20convenience.a -Wl,--no-whole-archive  -L/builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/x86_64-redhat-linux/libstdc++-v3/libsupc++/.libs -L/builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/x86_64-redhat-linux/libstdc++-v3/src -L/builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/x86_64-redhat-linux/libstdc++-v3/src/.libs -lm -L/builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/./gcc -L/lib/../lib64 -L/usr/lib/../lib64 -lc -lgcc_s /builddir/build/BUILD/gcc-11.2.1-20210728/obj-x86_64-redhat-linux/./gcc/crtendS.o /lib/../lib64/crtn.o  -Wl,-O1 -Wl,-z -Wl,relro -Wl,--gc-sections -Wl,--version-script=libstdc++-symbols.ver   -Wl,-soname -Wl,libstdc++.so.6 -o .libs/libstdc++.so.6.0.29

gcc --shared -o libstdc++.so.6 -shared-libgcc -nostdinc++ -nodefaultlibs -nostdlib \
  -L ./opt/rh/devtoolset-11/root/usr/lib/gcc/x86_64-redhat-linux/11/ -fPIC \
  /usr/lib/gcc/x86_64-redhat-linux/4.8.5/../../../../lib64/crti.o \
  /usr/lib/gcc/x86_64-redhat-linux/4.8.5/crtbeginS.o \
  -Wl,--whole-archive -lstdc++ -Wl,--no-whole-archive \
  -ldl -lm -lc -lgcc_s \
  -Wl,-O1 -Wl,-z -Wl,relro -Wl,--gc-sections \
  /usr/lib/gcc/x86_64-redhat-linux/4.8.2/crtendS.o /usr/lib64/crtn.o \
  -Wl,--version-script=libstdc++-symbols.ver -Wl,-soname -Wl,libstdc++.so.6

rm -rf opt *.rpm

popd

mv -vf /tmp/libstdc++.so.6 ./third_party/llvm-build/Release+Asserts/lib/
