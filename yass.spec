Name:    yass
Version: 1.0.0
Release: 1%{?dist}
Summary: lightweight and secure http/socks4/socks5 proxy

License: GPLv2
URL: https://github.com/Chilledheart/yass
Source0: https://github.com/Chilledheart/yass/archive/refs/tags/%{version}.tar.gz
%if 0%{?rhel} < 9 || 0%{?fedora}
BuildRequires: perl, libatomic-static
%endif
%if 0%{?rhel} >= 8 || 0%{?fedora}
BuildRequires: cmake >= 3.12, pkg-config
%endif
%if 0%{?rhel} == 7
BuildRequires: cmake3 >= 3.12, pkgconfig
%endif
BuildRequires: ninja-build, gcc, gcc-c++, golang >= 1.4
BuildRequires: glib2-devel, gtk3-devel

%description
Yet Another Shadow Socket is a lightweight and secure http/socks4/socks5 proxy for
embedded devices and low end boxes.

%prep
%setup -q -n %{name}-%{version}

%build
mkdir build
cd build
ENABLE_LLD=on
[ "a$DISABLE_LLD" != "a" ] && ENABLE_LLD=off
cmake3 -G Ninja -DBUILD_TESTS=on -DCMAKE_BUILD_TYPE=Release -DUSE_HOST_TOOLS=on -DGUI=on -DCLI=on -DSERVER=on -DENABLE_LLD="$ENABLE_LLD" ..
ninja yass yass_cli yass_server
cd ..

%check
ninja -C build check

%install
cd build
cmake3 -DCMAKE_INSTALL_PREFIX=%{buildroot}/usr ..
rm -rf %{buildroot}
ninja install
cd ..

%post
update-desktop-database

%files
%defattr(-,root,root)
%license LICENSE
%dir /usr/share/icons/hicolor/
%dir /usr/share/applications/
%dir /usr/share/pixmaps/
%{_bindir}/yass
%{_datadir}/applications/yass.desktop
%{_datadir}/pixmaps/yass.png
%{_datadir}/icons/hicolor/16x16/apps/yass.png
%{_datadir}/icons/hicolor/22x22/apps/yass.png
%{_datadir}/icons/hicolor/24x24/apps/yass.png
%{_datadir}/icons/hicolor/32x32/apps/yass.png
%{_datadir}/icons/hicolor/48x48/apps/yass.png
%{_datadir}/icons/hicolor/128x128/apps/yass.png
%{_datadir}/icons/hicolor/256x256/apps/yass.png
%{_datadir}/icons/hicolor/512x512/apps/yass.png

%package server
Summary: lightweight and secure http/socks4/socks5 proxy (server side)

%description server
Yet Another Shadow Socket is a lightweight and secure http/socks4/socks5 proxy for
embedded devices and low end boxes.

%files server
%defattr(-,root,root)
%{_bindir}/yass_server

%package client
Summary: lightweight and secure http/socks4/socks5 proxy (client side)

%description client
Yet Another Shadow Socket is a lightweight and secure http/socks4/socks5 proxy for
embedded devices and low end boxes.

%files client
%defattr(-,root,root)
%{_bindir}/yass_cli

%changelog
* Sat Jan 22 2022 Chilledheart <rwindz0@gmail.com> - 1.0.0-1
  - Initial release. (Closes: #4)
