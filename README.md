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
- Android [download apk](https://github.com/Chilledheart/yass/releases/download/1.9.1/yass-android-release-arm64-1.9.1.apk) or [download 32-bit apk](https://github.com/Chilledheart/yass/releases/download/1.9.1/yass-android-release-arm-1.9.1.apk)
- iOS [join via TestFlight](https://testflight.apple.com/join/6AkiEq09) or [download ipa](https://github.com/Chilledheart/yass/releases/download/1.9.1/yass-ios-release-arm64-1.9.1.ipa)
- Windows [download installer](https://github.com/Chilledheart/yass/releases/download/1.9.1/yass-mingw-winxp-release-x86_64-1.9.1-system-installer.exe) or [download 32-bit installer](https://github.com/Chilledheart/yass/releases/download/1.9.1/yass-mingw-winxp-release-i686-1.9.1-system-installer.exe) [(require vc 2010 runtime)][vs2010_x86] or [download woa arm64 installer](https://github.com/Chilledheart/yass/releases/download/1.9.1/yass-mingw-release-aarch64-1.9.1-system-installer.exe)
- macOS [download intel dmg](https://github.com/Chilledheart/yass/releases/download/1.9.1/yass-macos-release-x64-1.9.1.dmg) or [download apple silicon dmg](https://github.com/Chilledheart/yass/releases/download/1.9.1/yass-macos-release-arm64-1.9.1.dmg)
- Linux [download rpm](https://github.com/Chilledheart/yass/releases/download/1.9.1/yass.el7.x86_64.1.9.1-0.rpm) or [download deb](https://github.com/Chilledheart/yass/releases/download/1.9.1/yass-client-ubuntu-16.04-xenial_amd64.1.9.1.deb)

View more at [release page](https://github.com/Chilledheart/yass/releases/tag/1.9.1)

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
#### Fix disk space occupation issue for mobile users
Starting with release 1.9.2 and 1.8.5, it will no longer keep log file and will not occupy extra disk space for Android/iOS/HarmonyOS client.
#### Android vpn service support
Android releases from 1.5.24 are signed and have been updated to add [VpnService](https://developer.android.com/reference/android/net/VpnService) support.
#### iOS packet tunnel support
iOS releases from 1.5.22 have fixed memory pressure crashes and have been updated to add [Packet tunnel](https://developer.apple.com/documentation/networkextension/packet_tunnel_provider?language=objc) support.
#### macOS packet tunnel support (M1/M2/M3 only)
For Apple Silicon macOS Users such as M1/M2/M3, you can also install [Packet tunnel](https://developer.apple.com/documentation/networkextension/packet_tunnel_provider?language=objc) version via [TestFlight](https://testflight.apple.com/join/6AkiEq09).
#### Notarized macOS releases
macOS releases from 1.5.19 are [notarized](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution). Please note the dmg suffixed with `-unsigned` is not.
#### MinGW Build (alias Window Build)
MinGW 64-bit releases from 1.6.5 and 1.7.1 no longer require Visual C++ 2010 Runtime.

For Windows XP Users, please make Visual C++ 2010 Runtime installed and use 32-bit releases.
Depending the your system's architecture, use [x86 installer][vs2010_x86] or [x64 installer][vs2010_x64].
#### Supplementary support for missing ISRG (Let's Encrypt Root) on some Windows 11 Installation and Android prior to 7.1.1
Releases from 1.5.25 and 1.6.4 fixes an issue that ISRG Root 1 and ISRG Root 2 might be missing in some machines and that causes troubles.
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
