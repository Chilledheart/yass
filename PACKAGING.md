# Packaging Instruments

## Prerequisite
In addition, make sure you have these packages INSTALLED on your system.
- MinGW: [NSIS] and [7Zip]
- MSVC: [NSIS], [7Zip] and [wixtoolset][wix3] (put installation path in PATH environment variable)
- macOS: [Xcode]
- Debian: fakeroot devscripts debhelper clang
- Fedora and Fedora-like: rpm-build rpm-devel rpmlint rpmdevtools clang

## Windows (MinGW)/Packaging

Run in `Git Bash` in Start Menu:
```
./scripts/build-mingw.sh
```

## Windows (MSVC)/Packaging

Run `x64 Native Tools Command Prompt for VS 2019 (or 2022)` in Start Menu:
```
cd tools
go build
cd ..
./tools/build
```

## macOS/Packaging

Run in `Terminal`:
```
cd tools
go build
cd ..
./tools/build
```

## Debian/Packaging

Generate Packages under parent directory
```
./scripts/build-deb.sh
```

## Debian/Gtk3/Packaging
Generate Packages under parent directory
```
DEB_BUILD_PROFILES=gtk3 ./scripts/build-deb.sh
```

## Debian/Gtk4/Packaging
Generate Packages under parent directory
```
DEB_BUILD_PROFILES=gtk4 ./scripts/build-deb.sh
```

## Debian/Qt5/Packaging
Generate Packages under parent directory
```
DEB_BUILD_PROFILES=qt5 ./scripts/build-deb.sh
```

## Debian/Qt6/Packaging
Generate Packages under parent directory
```
DEB_BUILD_PROFILES=qt6 ./scripts/build-deb.sh
```

## Fedora/RHEL/CentOS/AlmaLinux/Rocky Gtk3 Packaging

Generate Packages under current directory
```
./scripts/build-rpm.sh
```

## Fedora/RHEL/CentOS/AlmaLinux/Rocky Gtk4 Packaging

Generate Packages under current directory
```
USE_GTK4=1 ./scripts/build-rpm.sh
```

## Fedora/RHEL/CentOS/AlmaLinux/Rocky Qt5 Packaging

Generate Packages under current directory
```
USE_QT5=1 ./scripts/build-rpm.sh
```

## Fedora/RHEL/CentOS/AlmaLinux/Rocky Qt6 Packaging

Generate Packages under current directory
```
USE_QT6=1 ./scripts/build-rpm.sh
```

## FreeBSD/Packaging

Generate Packages under current directory
```
cd tools
go build
cd ..
./tools/build
```

## Android/Packaging
See [android's README.md](android/README.md)

## HarmonyOS/Packaging
See [harmonyOS's README.md](harmony/README.md)

## iOS/Packaging
Make sure you have [Xcode] installed on your system.

TBD

## Flatpak/Packaging

Make sure you follow the [setup guide for your Linux distribution][flatpak_setup] before building
```
cd flatpak
flatpak remote-add --if-not-exists --user flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak-builder --force-clean --user --install-deps-from=flathub --ccache --mirror-screenshots-url=https://dl.flathub.org/media/ --repo=repo builddir io.github.chilledheart.yass.yml
flatpak build-bundle repo yass.flatpak io.github.chilledheart.yass --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo
```

[NSIS]: https://nsis.sourceforge.io/Download
[7Zip]: https://www.7-zip.org/
[wix3]: https://wixtoolset.org/docs/wix3/
[Xcode]: https://apps.apple.com/us/app/xcode/id497799835?mt=12
[flatpak_setup]: https://flathub.org/setup
