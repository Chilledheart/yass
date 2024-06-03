# Yet Another Shadow Socket

[![License](https://img.shields.io/github/license/Chilledheart/yass)][license-link]
[![Language: C++](https://img.shields.io/github/languages/top/Chilledheart/yass.svg)](https://github.com/Chilledheart/yass/search?l=cpp)
[![GitHub release (latest by SemVer)](https://img.shields.io/github/downloads/Chilledheart/yass/latest/total)](https://github.com/Chilledheart/yass/releases/latest)

[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Chilledheart/yass)](https://github.com/Chilledheart/yass/releases)
[![homebrew cask](https://img.shields.io/homebrew/cask/v/yass)](https://formulae.brew.sh/cask/yass)

[![aur yass-proxy](https://img.shields.io/aur/version/yass-proxy)](https://aur.archlinux.org/packages/yass-proxy)
[![aur yass-proxy-gtk3](https://img.shields.io/aur/version/yass-proxy-gtk3)](https://aur.archlinux.org/packages/yass-proxy-gtk3)
[![aur yass-proxy-qt6](https://img.shields.io/aur/version/yass-proxy-qt6)](https://aur.archlinux.org/packages/yass-proxy-qt6)
[![aur yass-proxy-cli](https://img.shields.io/aur/version/yass-proxy-cli)](https://aur.archlinux.org/packages/yass-proxy-cli)

yass is an efficient forward proxy client supporting http/socks4/socks4a/socks5 protocol running on PC and mobile devices.

## License
[GPLv2-only][license-link]

## Features

### Android/iOS/macOS (M1/M2/M3 only) comes with VPN Service support
Mobile users including macOS (M1/M2/M3) machines use YASS as VPN Service more than Global Proxy.

See [Supporteded Operating System](https://github.com/Chilledheart/yass/wiki/Supported-Operating-System) for more.
See [Status of Package Store](https://github.com/Chilledheart/yass/wiki/Status-of-Package-Store) for more.

### Post Quantum Kyber Support
Post Quantum Kyber Support (not enabled by default) is added on all of supported Platforms.

See [Protecting Chrome Traffic with Hybrid Kyber KEM](https://blog.chromium.org/2023/08/protecting-chrome-traffic-with-hybrid.html) for more.

### Prebuilt binaries
- Android [download apk](https://github.com/Chilledheart/yass/releases/download/1.10.4/yass-android-release-arm64-1.10.4.apk) or [download 32-bit apk](https://github.com/Chilledheart/yass/releases/download/1.10.4/yass-android-release-arm-1.10.4.apk)
- iOS [join via TestFlight](https://testflight.apple.com/join/6AkiEq09)
- Windows [download installer](https://github.com/Chilledheart/yass/releases/download/1.10.4/yass-mingw-win7-release-x86_64-1.10.4-system-installer.exe) [(require KB2999226 below windows 10)][KB2999226] or [download 32-bit installer](https://github.com/Chilledheart/yass/releases/download/1.10.4/yass-mingw-winxp-release-i686-1.10.4-system-installer.exe) [(require vc 2010 runtime)][vs2010_x86] or [download woa arm64 installer](https://github.com/Chilledheart/yass/releases/download/1.10.4/yass-mingw-release-aarch64-1.10.4-system-installer.exe)
- macOS [download intel dmg](https://github.com/Chilledheart/yass/releases/download/1.10.4/yass-macos-release-x64-1.10.4.dmg) or [download apple silicon dmg](https://github.com/Chilledheart/yass/releases/download/1.10.4/yass-macos-release-arm64-1.10.4.dmg)
> via homebrew: `brew install --cask yass`
- Linux [download rpm](https://github.com/Chilledheart/yass/releases/download/1.10.4/yass.el7.x86_64.1.10.4.rpm) or [download deb](https://github.com/Chilledheart/yass/releases/download/1.10.4/yass-ubuntu-16.04-xenial_amd64.1.10.4.deb)

View more at [Release Page](https://github.com/Chilledheart/yass/releases/tag/1.10.4)

### NaïveProxy-Compatible Protocol Support
Cipher http2 and https are NaïveProxy-compatible.

See [NaïveProxy](https://github.com/klzgrad/naiveproxy)'s project homepage for support.

## Usages
Visit wiki's [Usages](https://github.com/Chilledheart/yass/wiki/Usage).

## Build from Source
Take a look at [instructions](BUILDING.md).

## Sponsor Me
Please visit [the pages site](https://letshack.info).

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

## Additional Features

### SOCKS cipher Support
Experimental socks4/socks4a/socks5/socks5h cipher support is added for both of CLI and GUI.

### DoH (DNS over HTTPS) and DoT (DNS over TLS) Support
Experimental DoH and DoT support is added for both of CLI and GUI.

### Supplementary Support for ISRG Root X2 and ISRG Root X1 ca which is missing on some machines
These ca certificates are provided in both builtin ca bundle support and supplementary ca bundle support (bundled).

### Supplementary Support for DigiCert Global Root G2 ca which is missing on some machines
These ca certificates are provided in both builtin ca bundle support and supplementary ca bundle support (bundled).

### Specify Rate Limit (Command Line only)
Pass `--limit_rate rate` to command line.
Limits the _rate_ of response transmission to a client.
Uint can be `(none)`, `k` and `m`.

### Specify TCP Congestion Algorithm (Command Line only)
Pass `--congestion_algorithm algo` to command line.
Specify _algo_ as TCP congestion control algorithm for underlying TCP connections (Linux only).
See more at manpage [_tcp(7)_](https://linux.die.net/man/7/tcp)

### Use custom CA (Command Line only)
Pass `--certificate_chain_file file` to command line.
Use custom certificate chain provided by _file_ to verify server's certificate.

### Use server Side Support (Commmand Line only)
All ciphers supported by client are also supported by `yass_server`.
See more at manpage _yass_server(1)_

See [Server Usage](https://github.com/Chilledheart/yass/wiki/Usage:-server-setup) for more.

[license-link]: LICENSE
[KB2999226]: https://support.microsoft.com/en-us/topic/update-for-universal-c-runtime-in-windows-c0514201-7fe6-95a3-b0a5-287930f3560c
[vs2010_x86]: https://download.microsoft.com/download/1/6/5/165255E7-1014-4D0A-B094-B6A430A6BFFC/vcredist_x86.exe
