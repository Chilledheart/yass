# Yet Another Shadow Socket

[![License](https://img.shields.io/github/license/Chilledheart/yass)][license-link]
[![Language: C++](https://img.shields.io/github/languages/top/Chilledheart/yass.svg)](https://github.com/Chilledheart/yass/search?l=cpp)
[![GitHub release (latest by SemVer)](https://img.shields.io/github/downloads/Chilledheart/yass/latest/total)](https://github.com/Chilledheart/yass/releases/latest)

[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Chilledheart/yass)](https://github.com/Chilledheart/yass/releases)
[![aur yass-proxy](https://img.shields.io/aur/version/yass-proxy)](https://aur.archlinux.org/packages/yass-proxy)
[![aur yass-proxy-cli](https://img.shields.io/aur/version/yass-proxy-cli)](https://aur.archlinux.org/packages/yass-proxy-cli)

yass is client-server model based and efficient forward proxy
supporting http/socks4/socks4a/socks5 protocol. The server side is experimental.

## Features

- Better scaled, less memory consumption and CPU cycles.
- Easier to maintain and adopt new features.
- Safe memory layout.

More Information refers to [wiki](https://github.com/Chilledheart/yass/wiki)

## Supported Operating System
- Android (VpnService support) [download apk](https://github.com/Chilledheart/yass/releases/download/1.5.17/yass-android-release-arm64-1.5.17.apk)
- iOS (Packet Tunnel support, iPhone/iPad) [TestFlight Invite URL](https://testflight.apple.com/join/6AkiEq09)
- Windows [download zip](https://github.com/Chilledheart/yass/releases/download/1.5.17/yass-mingw-winxp-release-i686-1.5.17.zip)
- macOS [intel dmg](https://github.com/Chilledheart/yass/releases/download/1.5.17/yass-macos-release-x64-1.5.17.dmg) or [apple silicon dmg](https://github.com/Chilledheart/yass/releases/download/1.5.17/yass-macos-release-arm64-1.5.17.dmg)
- Linux (including OpenWRT, Ubuntu, CentOS, OpenSUSE Leap, Alpine Linux and etc) [download gtk](https://github.com/Chilledheart/yass/releases/download/1.5.17/yass-linux-release-amd64-1.5.17.tgz) [download ipk](https://github.com/Chilledheart/yass/releases/download/1.5.17/yass-cli_linux-openwrt-release-x86_64-1.5.17-1_x86_64.ipk)
- FreeBSD [download gtk for freebsd 14](https://github.com/Chilledheart/yass/releases/download/1.5.17/yass-freebsd14-release-amd64-1.5.17.tgz)

## Build from Source
Take a look at [BUILDING.md] for more instructions.

## Sponsor Me
Please visit [the pages site](https://letshack.info).

## License
It is licensed with [GPLv2][license-link].

## Usages
Please visit to [wiki](https://github.com/Chilledheart/yass/wiki) such as [Usages](https://github.com/Chilledheart/yass/wiki/Usage) and [Server Usage](https://github.com/Chilledheart/yass/wiki/Usage:-server-setup)

## Build Status

[![Compiler Compatibility](https://github.com/Chilledheart/yass/actions/workflows/compiler.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/compiler.yml)
[![CircleCI](https://img.shields.io/circleci/build/github/Chilledheart/yass/develop?logo=circleci&&label=Sanitizers%20and%20Ubuntu%20arm)](https://circleci.com/gh/Chilledheart/yass/?branch=develop)
[![Cirrus CI](https://img.shields.io/cirrus/github/Chilledheart/yass/develop?logo=cirrusci&&label=FreeBSD%20and%20macOS)](https://cirrus-ci.com/github/Chilledheart/yass/develop)

[![MinGW Build](https://github.com/Chilledheart/yass/actions/workflows/releases-mingw-new.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-mingw-new.yml)
[![Linux Build](https://github.com/Chilledheart/yass/actions/workflows/releases-linux-binary.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-linux-binary.yml)
[![macOS Build](https://github.com/Chilledheart/yass/actions/workflows/releases-macos.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-macos.yml)

[![Android Build](https://github.com/Chilledheart/yass/actions/workflows/releases-android-binary.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-android-binary.yml)
[![iOS Build](https://github.com/Chilledheart/yass/actions/workflows/releases-ios.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-ios.yml)

[![OpenWRT Build](https://github.com/Chilledheart/yass/actions/workflows/releases-openwrt-binary.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-openwrt-binary.yml)
[![FreeBSD Build](https://github.com/Chilledheart/yass/actions/workflows/releases-freebsd-binary.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-freebsd-binary.yml)
[![RPM Build](https://github.com/Chilledheart/yass/actions/workflows/releases-rpm.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-rpm.yml)
[![DEB Build](https://github.com/Chilledheart/yass/actions/workflows/releases-deb.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-deb.yml)

[![MSVC Build](https://github.com/Chilledheart/yass/actions/workflows/releases-windows.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-windows.yml)
[![Old MinGW Build](https://github.com/Chilledheart/yass/actions/workflows/releases-mingw.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-mingw.yml)

[license-link]: LICENSE

[HTTP2]: https://datatracker.ietf.org/doc/html/rfc9113

[BUILDING.md]: BUILDING.md
