# Yet Another Shadow Socket

[![License][license-svg]][license-link]
[![GitHub all downloads](https://img.shields.io/github/downloads/Chilledheart/yass/total)](https://github.com/Chilledheart/yass/releases)
[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Chilledheart/yass)](https://github.com/Chilledheart/yass/releases)
[![GitHub latest downloads](https://img.shields.io/github/downloads/Chilledheart/yass/1.3.7/total)](https://github.com/Chilledheart/yass/releases/tag/1.3.7)

[![Compiler Compatibility](https://github.com/Chilledheart/yass/actions/workflows/compiler.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/compiler.yml)
[![Sanitizers](https://github.com/Chilledheart/yass/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/sanitizers.yml)
[![CircleCI](https://circleci.com/gh/Chilledheart/yass/tree/develop.svg?style=svg)](https://circleci.com/gh/Chilledheart/yass/?branch=develop)
[![Cirrus CI](https://img.shields.io/cirrus/github/Chilledheart/yass/develop?logo=cirrusci&&label="FreeBSD%20and%20macOS%20arm64")](https://cirrus-ci.com/github/Chilledheart/yass/develop)

[![Windows Build](https://github.com/Chilledheart/yass/actions/workflows/releases-windows.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-windows.yml)
[![macOS Build](https://github.com/Chilledheart/yass/actions/workflows/releases-macos.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-macos.yml)
[![MinGW Build](https://github.com/Chilledheart/yass/actions/workflows/releases-mingw.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-mingw.yml)

[![Linux Build](https://github.com/Chilledheart/yass/actions/workflows/releases-linux-binary.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-linux-binary.yml)
[![FreeBSD Build](https://github.com/Chilledheart/yass/actions/workflows/releases-freebsd-binary.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-freebsd-binary.yml)
[![RPM Build](https://github.com/Chilledheart/yass/actions/workflows/releases-rpm.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-rpm.yml)
[![DEB Build](https://github.com/Chilledheart/yass/actions/workflows/releases-deb.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-deb.yml)

Yet Another Shadow Socket is lightweight and secure http/socks4/socks5 proxy.

<!-- TOC -->

- [Usage](#usage)
- [Features](#features)
- [Ciphers](#ciphers)
  * [HTTP2 Tunnel Support](#http2-tunnel-support)
  * [NaiveProxy Protocol Support](#naiveproxy-protocol-support)
  * [Shadowsocks PC-friendly Ciphers](#shadowsocks-pc-friendly-ciphers)
  * [Shadowsocks mobile-friendly Ciphers](#shadowsocks-mobile-friendly-ciphers)
- [Supported Operating System](#supported-operating-system)
  * [Screenshot on HTTP2 support](#screenshot-on-http2-support)
  * [Screenshot on macOS](#screenshot-on-macos)
  * [Screenshot on Windows 11](#screenshot-on-windows-11)
  * [Screenshot on Windows 7](#screenshot-on-windows-7)
  * [Screenshot on Windows XP](#screenshot-on-windows-xp)
  * [Screenshot on Ubuntu 20.04](#screenshot-on-ubuntu-2004)
  * [Screenshot on CentOS 7](#screenshot-on-centos-7)
  * [Screenshot on FreeBSD](#screenshot-on-freebsd)
- [Client Protocols Supported](#client-protocols-supported)
- [Build from Source](#build-from-source)
- [License](#license)

<!-- /TOC -->

## Usage

0. Please have your server set up (for example, you can give it a visit: [Naiveproxy Server setup][naiveproxy-server], the server side is compatible)
> Please note you must buy from any VPS provider such as AMAZON, Azure, Google Compute Cloud and etc to get a VPS running naiveproxy/yass server.
> Read it carefully, don't omit things important but not stated such as buying a domain name (used to do TLS extention, http2 requirement)
>
> Ask your friends if you don't know how to setup a naive/yass server. We don't provide any service, either free or paid.
>
> otherwise you can switch to weaker ciphers other than http2
> so you can use your IP address as server's host address directly.

1. Download the package (and install if it is a installer) and run YASS.
It is a GUI application, for a quick start we can pick from here:

- Windows XP SP3 or later ([Setup installer](https://github.com/Chilledheart/yass/releases/download/1.3.7/yass-winxp-release-x86-static-1.3.7-installer.exe))
- Ubuntu 16.04 or later ([DEB](https://github.com/Chilledheart/yass/releases/download/1.3.7/yass-ubuntu-16.04-xenial_amd64.1.3.7.deb))
- CentOS 7 or later ([RPM](https://github.com/Chilledheart/yass/releases/download/1.3.7/yass-centos-7.el7.x86_64.1.3.7.rpm))
- macOS 10.14 or later ([DMG image](https://github.com/Chilledheart/yass/releases/download/1.3.7/yass-macos-release-universal-1.3.7.dmg))

> Visit the [release page](https://github.com/Chilledheart/yass/releases/tag/1.3.7) for other flavours such as tarball
downloads or packages running on aarch64/arm64 hardware.

2. In YASS windows, please feed in Server's Host (domain name), Server Port,
Username and Password used in previous step, changing Cipher Method to `http2`,
Local Host to `127.0.0.1`, Local Port to `1081` and Timeout to `0`.

3. Click your platform's internet options to use HTTP PROXY on `127.0.0.1` port to `1081` and
then start your browser just as you visit the websites directly from the server.
You can use this software as SOCKS4/SOCKS5 PROXY on the SAME port!

> As REDIR mode usage in middle box for advanced users, please make sure you pass `--redir_mode` argument to the software.
> and local host to `0.0.0.0` to receive requests other that the current machine.

## Features

- Better scale, less memory consumption and CPU cycles, less than 0.1% on my iMac.
- Easier to maintain and adopt new features, so far [aead][aead] ciphers supported.
- Safe memory layout, tested against [Address Sanitizer][asan].
- Thread Safe, tested against [Thread Sanitizer][tsan].
- IPv6 compatible (both client and server side)

## Ciphers
### HTTP2 Tunnel Support
- [x] Basic [HTTP2] Support
- [x] HTTPS1.1 fallback support (both server and client sides)
- [x] Padding Support

More Information refers to https://github.com/Chilledheart/yass/issues/55

### NaiveProxy Protocol Support
Notable missing features to be done compared with [naiveproxy]
- [x] Caddy + forwardproxy Support (see [Server setup for naive fork][naiveproxy-server])
- [x] Proxy payload padding (see Padding Support)
- [ ] H2 RST_STREAM frame padding
- [x] H2 HEADERS frame padding
- [x] Opt-in of padding protocol
- [ ] Support HTTP/2 and HTTP/3 CONNECT tunnel Fast Open using the `fastopen`
  header
- [x] No performance degrade compared to [naiveproxy] client

Server-side features compared with caddy+forwardproxy
- [x] Basic Authentification
- [x] Opt-in of padding protocol
- [x] Via-IP support (HTTPS 1.1 only)
- [x] Via-via support (HTTPS 1.1 only)
- [ ] File Server fallback support

### Shadowsocks PC-friendly Ciphers
- [x] [AES_128_GCM][aes128gcm]
- [x] [AES_256_GCM][aes256gcm]
- [x] [AES_128_GCM_12][aes128gcm12]
- [x] [AES_192_GCM][aes192gcm] (Not recommended)

### Shadowsocks mobile-friendly Ciphers
- [x] [CHACHA20_POLY1305][chacha20]
- [x] [XCHACHA20_POLY1305][xchacha20]

## Supported Operating System
- macOS (Mac OS X 10.14 or later, macOS 11.0 or later, Apple Silicon supported)
- Linux (CentOS 7 or later, Debian 8 or later, Ubuntu 14.04 or later)
- Windows (Windows 8.1 or later, Windows XP SP3/Windows 7 binaries also provided)

### Screenshot on HTTP2 support
<img width="484" alt="snapshot-http2" src="https://github.com/Chilledheart/yass/assets/54673341/99db123b-6d8a-418f-b1ea-8a2844702348">

### Screenshot on macOS
<img width="484" alt="snapshot-macos" src="https://user-images.githubusercontent.com/54673341/223671134-a42974b5-9801-4999-bbbf-740e409ae708.png">

### Screenshot on Windows 11
<img width="484" alt="snapshot-win11" src="https://user-images.githubusercontent.com/54673341/223904001-c56e1fbc-26d1-4f67-863b-4d6d0a032da6.png">

### Screenshot on Windows 7
<img width="484" alt="snapshot-win7" src="https://user-images.githubusercontent.com/54673341/223904030-9e404d12-924d-42f7-988e-83e9433aa173.png">

### Screenshot on Windows XP
<img width="484" alt="snapshot-winxp" src="https://user-images.githubusercontent.com/54673341/223904060-5e84a623-337f-4446-9a18-d710f2089dea.png">

### Screenshot on Ubuntu 20.04
<img width="484" alt="snapshot-ubuntu" src="https://user-images.githubusercontent.com/54673341/223904103-b20361a4-b704-4575-9244-9839b3aa3384.png">

### Screenshot on CentOS 7
<img width="484" alt="snapshot-centos" src="https://user-images.githubusercontent.com/54673341/223904439-a8187be2-17a1-43a4-81e4-2258db36c690.png">

### Screenshot on FreeBSD
<img width="484" alt="snapshot-freebsd" src="https://user-images.githubusercontent.com/54673341/223905714-14c18b17-e11a-4f63-a8fb-83522f04cbed.png">

## Client Protocols Supported
- Socks4 Proxy
- Socks4A Proxy
- Socks5/Socks5h Proxy
- HTTP Proxy

## Build from Source
Looking for how to build from source?
Take a look at [BUILDING.md] for more instructions.

## License
It is licensed with [GPLv2][license-link].

[license-svg]: https://img.shields.io/badge/license-GPL2-lightgrey.svg
[license-link]: LICENSE

[aead]: https://shadowsocks.org/en/wiki/AEAD-Ciphers.html
[asan]: https://github.com/google/sanitizers/wiki/AddressSanitizer
[tsan]: https://github.com/google/sanitizers/wiki#threadsanitizer
[HTTP2]: https://datatracker.ietf.org/doc/html/rfc9113
[aes128gcm]: https://tools.ietf.org/html/rfc5116
[aes128gcm12]: https://tools.ietf.org/html/rfc5282
[aes192gcm]: https://tools.ietf.org/html/rfc5282
[aes256gcm]: https://tools.ietf.org/html/rfc5116
[chacha20]: https://tools.ietf.org/html/rfc7539
[xchacha20]: https://en.wikipedia.org/wiki/ChaCha20-Poly1305#XChaCha20-Poly1305_%E2%80%93_extended_nonce_variant
[naiveproxy]: https://github.com/klzgrad/naiveproxy
[naiveproxy-server]: https://github.com/klzgrad/naiveproxy#server-setup

[BUILDING.md]: BUILDING.md
