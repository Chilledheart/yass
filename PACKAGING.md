# Packaging Instruments

## Prerequisite
In addition, make sure you have these packages INSTALLED on your system.
- MinGW: [NSIS]
- MSVC: [NSIS] and [wixtoolset][wix3] (put installation path in PATH environment variable)
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
export CC=clang
export CXX=clang++
./scripts/build-deb.sh
```

## Fedora/RHEL/CentOS/AlmaLinux/Rocky Linux Packaging

Generate Packages under current directory
```
export CC=clang
export CXX=clang++
./scripts/build-rpm.sh
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

[NSIS]: https://nsis.sourceforge.io/Download
[wix3]: https://wixtoolset.org/docs/wix3/
[Xcode]: https://apps.apple.com/us/app/xcode/id497799835?mt=12
