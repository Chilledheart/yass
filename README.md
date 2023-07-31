# Yet Another Shadow Socket

[![License][license-svg]][license-link]
[![GitHub all downloads](https://img.shields.io/github/downloads/Chilledheart/yass/total)](https://github.com/Chilledheart/yass/releases)
[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Chilledheart/yass)](https://github.com/Chilledheart/yass/releases)
[![GitHub latest downloads](https://img.shields.io/github/downloads/Chilledheart/yass/1.3.10/total)](https://github.com/Chilledheart/yass/releases/tag/1.3.10)

[![Compiler Compatibility](https://github.com/Chilledheart/yass/actions/workflows/compiler.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/compiler.yml)
[![CircleCI](https://img.shields.io/circleci/build/github/Chilledheart/yass/develop?logo=circleci&&label=Sanitizers%20and%20Ubuntu%20arm)](https://circleci.com/gh/Chilledheart/yass/?branch=develop)
[![Cirrus CI](https://img.shields.io/cirrus/github/Chilledheart/yass/develop?logo=cirrusci&&label=FreeBSD%20and%20macOS)](https://cirrus-ci.com/github/Chilledheart/yass/develop)

[![Windows Build](https://github.com/Chilledheart/yass/actions/workflows/releases-windows.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-windows.yml)
[![macOS Build](https://github.com/Chilledheart/yass/actions/workflows/releases-macos.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-macos.yml)
[![MinGW Build](https://github.com/Chilledheart/yass/actions/workflows/releases-mingw.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-mingw.yml)

[![Linux Build](https://github.com/Chilledheart/yass/actions/workflows/releases-linux-binary.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-linux-binary.yml)
[![FreeBSD Build](https://github.com/Chilledheart/yass/actions/workflows/releases-freebsd-binary.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-freebsd-binary.yml)
[![RPM Build](https://github.com/Chilledheart/yass/actions/workflows/releases-rpm.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-rpm.yml)
[![DEB Build](https://github.com/Chilledheart/yass/actions/workflows/releases-deb.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-deb.yml)

Yet Another Shadow Socket is client-server model based and efficient forward proxy
supporting http/socks4/socks4a/socks5 protocol via https/http2 tunnel in the air.

<!-- TOC -->

- [Features](#features)
- [Client Protocols Supported](#client-protocols-supported)
- [Build from Source](#build-from-source)
- [Runtime requirements on Mingw](#runtime-requirements-on-mingw)
- [License](#license)

<!-- /TOC -->

## Features

- Better scale, less memory consumption and CPU cycles, less than 0.1% on my iMac.
- Easier to maintain and adopt new features, so far [aead][aead] ciphers supported.
- Safe memory layout, tested against [Address Sanitizer][asan].
- Thread Safe, tested against [Thread Sanitizer][tsan].
- IPv6 compatible (both client and server side)

More Information refers to [wiki](https://github.com/Chilledheart/yass/wiki)

## Supported Operating System
- macOS (Mac OS X 10.14 or later, macOS 11.0 or later, Apple Silicon supported)
- Linux (CentOS 7 or later, Debian 8 or later, Ubuntu 14.04 or later)
- Windows (Windows 8.1 or later, Windows XP SP3/Windows 7 binaries also provided)

## Client Protocols Supported
- [x] Socks4 Proxy
- [x] Socks4A Proxy
- [x] Socks5/Socks5h Proxy
- [x] HTTP Proxy
- [ ] HTTPS Proxy

## Build from Source
Looking for how to build from source?
Take a look at [BUILDING.md] for more instructions.

## Runtime requirements on Mingw
If you are using windows operating system, there is no runtime requirement mostly. However, if you choose to use mingw
binaries, it is necessary that you must make sure your OS has visual c++ 2010 runtime. For your reference, here is [x86
installer][vs2010_x86] download url and here is [x64 installer][vs2010_x64] one.

## Sponsor Me
Please visit [website](https://letshack.info).

## License
It is licensed with [GPLv2][license-link].

[license-svg]: https://img.shields.io/badge/license-GPL2-lightgrey.svg
[license-link]: LICENSE

[aead]: https://shadowsocks.org/en/wiki/AEAD-Ciphers.html
[asan]: https://github.com/google/sanitizers/wiki/AddressSanitizer
[tsan]: https://github.com/google/sanitizers/wiki#threadsanitizer
[HTTP2]: https://datatracker.ietf.org/doc/html/rfc9113


[BUILDING.md]: BUILDING.md
[vs2010_x64]: https://download.microsoft.com/download/1/6/5/165255E7-1014-4D0A-B094-B6A430A6BFFC/vcredist_x64.exe
[vs2010_x86]: https://download.microsoft.com/download/1/6/5/165255E7-1014-4D0A-B094-B6A430A6BFFC/vcredist_x86.exe
