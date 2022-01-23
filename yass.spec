Name:    yass
Version: 1.0.0
Release: 1%{?dist}
Summary: lightweight and secure http/socks4/socks5 proxy

License: GPLv2
URL: https://github.com/Chilledheart/yass
Source0: https://github.com/Chilledheart/yass/archive/refs/tags/%{version}.tar.gz
BuildRequires: cmake >= 3.8, ninja-build, pkg-config, perl, gcc >= 6.1, gcc-c++ >= 6.1, golang >= 1.4
BuildRequires: libunwind-devel, gtk3-devel, gtkmm30-devel
Requires: libunwind gtk3 gtkmm30

%description
Yet Another Shadow Socket is a lightweight and secure http/socks4/socks5 proxy for
embedded devices and low end boxes.

%prep
%setup -q -n %{name}-%{version}

%build
cd third_party/boringssl
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja crypto
cp -fv crypto/libcrypto.a ../libcrypto.a
cd ..
cd ../..

mkdir build
cd build
cmake -G Ninja -DGUI=on -DCLI=off -DSERVER=off ..
ninja yass
cd ..

%check
#nothing

%install
cd build
cmake -DCMAKE_INSTALL_PREFIX=%{buildroot}/usr ..
rm -rf %{buildroot}
ninja install
cd ..

%post
update-desktop-database

%files
%license GPL-2.0
%dir /usr/share/icons/hicolor/
%dir /usr/share/applications/
%dir /usr/share/pixmaps/
%{_bindir}/yass
%{_datadir}/applications/yass.desktop
%{_datadir}/pixmaps/network.png
%{_datadir}/icons/hicolor/16x16/apps/network.png
%{_datadir}/icons/hicolor/22x22/apps/network.png
%{_datadir}/icons/hicolor/24x24/apps/network.png
%{_datadir}/icons/hicolor/32x32/apps/network.png
%{_datadir}/icons/hicolor/48x48/apps/network.png
%{_datadir}/icons/hicolor/128x128/apps/network.png
%{_datadir}/icons/hicolor/256x256/apps/network.png
%{_datadir}/icons/hicolor/512x512/apps/network.png

%changelog
* Sat Jan 22 2022 Chilledheart <rwindz0@gmail.com> - 1.0.0-1
  - Initial release. (Closes: #4)
