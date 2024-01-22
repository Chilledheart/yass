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

More Information refers to [wiki](https://github.com/Chilledheart/yass/wiki) and [NaïveProxy][naiveproxy]-compatible protocol support.

## Usages

### Prebuilt binaries
- Android [download apk](https://github.com/Chilledheart/yass/releases/download/1.5.21/yass-android-release-arm64-1.5.21.apk)
- iOS [join via TestFlight](https://testflight.apple.com/join/6AkiEq09) or [download ipa](https://github.com/Chilledheart/yass/releases/download/1.5.21/yass-ios-release-arm64-1.5.21.ipa)
- Windows [download zip](https://github.com/Chilledheart/yass/releases/download/1.5.21/yass-mingw-winxp-release-i686-1.5.21.zip) or [download woa zip](https://github.com/Chilledheart/yass/releases/download/1.5.21/yass-mingw-release-aarch64-1.5.21.zip)
- macOS [download intel dmg](https://github.com/Chilledheart/yass/releases/download/1.5.21/yass-macos-release-x64-1.5.21.dmg) or [download apple silicon dmg](https://github.com/Chilledheart/yass/releases/download/1.5.21/yass-macos-release-arm64-1.5.21.dmg)
- Linux [download rpm](https://github.com/Chilledheart/yass/releases/download/1.5.21/yass-centos-7.el7.x86_64.1.5.21-0.rpm) or [download deb](https://github.com/Chilledheart/yass/releases/download/1.5.21/yass-client-ubuntu-16.04-xenial_amd64.1.5.21.deb)

View more at [release page](https://github.com/Chilledheart/yass/releases/tag/1.5.21)

### Status of Package Store
Visit wiki's [Status of Package Store](https://github.com/Chilledheart/yass/wiki/Status-of-Package-Store)

### Build from Source
Take a look at [BUILDING.md] for more instructions.

### Screenshots
Visit wiki's [Supported Operating System](https://github.com/Chilledheart/yass/wiki/Supported-Operating-System)

### Usages
Visit wiki's [Usages](https://github.com/Chilledheart/yass/wiki/Usage) and [Server Usage](https://github.com/Chilledheart/yass/wiki/Usage:-server-setup).

### Important notes before use

#### NaïveProxy Protocol Support
It refers to [http2 cipher](https://github.com/Chilledheart/yass/wiki/Supported-Operating-System#screenshot-on-na%C3%AFveproxy-support) as [NaïveProxy][naiveproxy]-compatible protocol support.
#### Notarized macOS releases
macOS Releases from 1.5.19 are [notarized](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution). Please note the dmg suffixed with `-unsigned` is not.
#### Experimental ios packet tunnel support
iOS releases from 1.5.21 are fixed memory pressure crash and have been updated to add [Packet tunnel](https://developer.apple.com/documentation/networkextension/packet_tunnel_provider?language=objc) support.
#### Experimental android vpn service support
Android releases have been updated to add [VpnService](https://developer.android.com/reference/android/net/VpnService) support.
#### MinGW Build (alias Window Build)
Please install Visual C++ 2010 runtime before use. Depending the your system's architecture, use [x86 installer][vs2010_x86] or [x64 installer][vs2010_x64].
#### MSVC Build(previous Windows Build) prebuilt binaries removed
Due to the fact of possibly violating GPL license by linking static Visual C++ Runtime or distribute the CRT dll files
we are still supporting MSVC builds, but the prebuilt binaries are removed from the release page.
You can build MSVC binaries by yourself or switch to MinGW builds.

## Sponsor Me
Please visit [the pages site](https://letshack.info).

## License
It is licensed with [GPLv2][license-link].

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
[naiveproxy]: https://github.com/klzgrad/naiveproxy
[HTTP2]: https://datatracker.ietf.org/doc/html/rfc9113
[vs2010_x64]: https://download.microsoft.com/download/1/6/5/165255E7-1014-4D0A-B094-B6A430A6BFFC/vcredist_x64.exe
[vs2010_x86]: https://download.microsoft.com/download/1/6/5/165255E7-1014-4D0A-B094-B6A430A6BFFC/vcredist_x86.exe
[BUILDING.md]: BUILDING.md
