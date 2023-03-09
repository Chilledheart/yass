# Yet Another Shadow Socket

[![License][license-svg]][license-link]
[![GitHub all downloads](https://img.shields.io/github/downloads/Chilledheart/yass/total)](https://github.com/Chilledheart/yass/releases)
[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Chilledheart/yass)](https://github.com/Chilledheart/yass/releases)
[![GitHub latest downloads](https://img.shields.io/github/downloads/Chilledheart/yass/1.2.7/total)](https://github.com/Chilledheart/yass/releases/tag/1.2.7)

[![Compiler Compatibility](https://github.com/Chilledheart/yass/actions/workflows/compiler.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/compiler.yml)
[![Sanitizers](https://github.com/Chilledheart/yass/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/sanitizers.yml)

[![Clang Tidy Results](https://github.com/Chilledheart/yass/actions/workflows/clang-tidy.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/clang-tidy.yml)

[![Windows Build](https://github.com/Chilledheart/yass/actions/workflows/releases-windows.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-windows.yml)
[![macOS Build](https://github.com/Chilledheart/yass/actions/workflows/releases-macos.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-macos.yml)
[![MinGW Build](https://github.com/Chilledheart/yass/actions/workflows/releases-mingw.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-mingw.yml)

[![Linux Build](https://github.com/Chilledheart/yass/actions/workflows/releases-linux-binary.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-linux-binary.yml)
[![FreeBSD Build](https://github.com/Chilledheart/yass/actions/workflows/releases-freebsd-binary.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-freebsd-binary.yml)
[![RPM Build](https://github.com/Chilledheart/yass/actions/workflows/releases-rpm.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-rpm.yml)
[![DEB Build](https://github.com/Chilledheart/yass/actions/workflows/releases-deb.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/releases-deb.yml)

Yet Another Shadow Socket is lightweight and secure http/socks4/socks5 proxy.

<!-- TOC -->

- [Features](#features)
- [Ciphers](#ciphers)
  * [HTTP2 Tunnel Support](#http2-tunnel-support)
  * [NaiveProxy Protocol Support](#naiveproxy-protocol-support)
  * [Shadowsocks PC-friendly Ciphers](#shadowsocks-pc-friendly-ciphers)
  * [Shadowsocks mobile-friendly Ciphers](#shadowsocks-mobile-friendly-ciphers)
- [Supported Operating System](#supported-operating-system)
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

## Features

- Better scale, less memory consumption and CPU cycles, less than 0.1% on my iMac.
- Easier to maintain and adopt new features, so far [aead][aead] ciphers supported.
- Safe memory layout, tested against [Address Sanitizer][asan].
- Thread Safe, tested against [Thread Sanitizer][tsan].

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
- [ ] Performance degrade compared to [naiveproxy] client

Server-side features compared with caddy+forwardproxy
- [x] Basic Authentification
- [x] Opt-in of padding protocol
- [ ] Via-IP support
- [ ] Via-via support
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
- Windows (Windows 8.1 or later, Windows XP/Windows 7 binaries also provided)

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
<img width="484" alt="snapshot-freebsd" src="https://user-images.githubusercontent.com/54673341/158511519-493f1a40-97c5-4748-a530-fe8e4b3c2ce1.jpg">

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
