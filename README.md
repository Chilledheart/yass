# Yet Another Shadow Socket

[![License][license-svg]][license-link]
[![GitHub all releases](https://img.shields.io/github/downloads/Chilledheart/yass/total)](https://github.com/Chilledheart/yass/releases)
[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Chilledheart/yass)](https://github.com/Chilledheart/yass/releases)

[![Windows Build](https://github.com/Chilledheart/yass/actions/workflows/windows.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/windows.yml)
[![macOS Build](https://github.com/Chilledheart/yass/actions/workflows/macos.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/macos.yml)
[![Linux Build](https://github.com/Chilledheart/yass/actions/workflows/linux.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/linux.yml)
[![Ubuntu Build](https://github.com/Chilledheart/yass/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/ubuntu.yml)

Yet Another Shadow Socket is lightweight and secure http/socks4/socks5 proxy.

<!-- TOC -->

- [Features](#features)
- [Ciphers](#ciphers)
  * [FIPS Ciphers](#fips-ciphers)
  * [Mobile-friendly Ciphers](#mobile-friendly-ciphers)
- [Supported Operating System](#supported-operating-system)
  * [Screenshot on Windows 11](#screenshot-on-windows-11-)
  * [Screenshot on Windows 7](#screenshot-on-windows-7-)
  * [Screenshot on Windows XP](#screenshot-on-windows-xp-)
  * [Screenshot on Ubuntu 16.04](#screenshot-on-ubuntu-1604)
- [Client Protocols Supported](#client-protocols-supported)
- [Build from Source](#build-from-source)
- [License](#license)
- [Running on Windows](#running-on-windows)
  * [Notes on Windows 10 or above](#notes-on-windows-10-or-above)
  * [Notes on Vista, Windows 7, Windows 8 and Windows 8.1](#notes-on-vista--windows-7--windows-8-and-windows-81)
  * [Notes on Windows XP](#notes-on-windows-xp)
- [Running on Ubuntu](#running-on-ubuntu)

<!-- /TOC -->

## Features

- Better scale, less memory consumption and CPU cycles, less than 0.1% on my iMac.
- Easier to maintain and adopt new features, so far [aead][aead] ciphers supported.
- Safe memory layout, tested against [Address Sanitizer][asan].

## Ciphers
### FIPS Ciphers
- [AEAD_AES_128_GCM][aes128gcm]
- [AEAD_AES_256_GCM][aes256gcm]
- [AEAD_AES_128_GCM_12][aes128gcm12]
- [AEAD_AES_192_GCM][aes192gcm]

### Mobile-friendly Ciphers
- [AEAD_CHACHA20_POLY1305][chacha20]
- [AEAD_XCHACHA20_POLY1305][chacha20]

Defined in [TLS 1.3][tls13]

## Supported Operating System
- macOS (MacOS 10.10 or later, macOS 11.0 or later, Apple Silicon supported)
- Linux (CentOS 8 or later, Debian 9 or later, Ubuntu 16.04 or later)
- Windows (Windows Vista or later, Windows XP package also supported)

### Screenshot on Windows 11
<img width="484" alt="snapshot-win11" src="https://user-images.githubusercontent.com/54673341/151115838-4deb128c-4c51-4a3c-9758-4f58da47984e.png">

### Screenshot on Windows 7
<img width="484" alt="snapshot-win7" src="https://user-images.githubusercontent.com/54673341/151115815-dd8cfdf4-f5b0-4313-956b-125c35a54d6f.png">

### Screenshot on Windows XP
<img width="484" alt="snapshot-winxp" src="https://user-images.githubusercontent.com/54673341/151115871-218610b6-413c-4c00-827b-3fdffc241b65.png">

### Screenshot on Ubuntu 16.04
<img width="484" alt="snapshot-winxp" src="https://user-images.githubusercontent.com/54673341/151591522-d86248f7-763f-432e-9dd8-fd16317d477b.png">

## Client Protocols Supported
- Socks4 Proxy
- Socks4A Proxy
- Socks5 Proxy
- HTTP Proxy

## Build from Source
Looking for how to build from source?
Take a look at [BUILDING.md] for more instructions.

## License
It is licensed with [GPLv2][license-link].

## Running on Windows

TLDR' if you running `yass.exe` ends with missing dlls error, please download
standalone packages (slightly larger) or install [Visual C++ Redistributable Package][latest-supported-vc-redist]
below. The required UCRT (Universal C Runtime) is bundled as well.

Also the packages named "static" are also runnable except you are probably
running without the updated [Visual C++ Redistributable Package][latest-supported-vc-redist]
and UCRT (via hotfixes or upgrade etc).

### Notes on Windows 10 or above

[Visual C++ Redistributable Package][latest-supported-vc-redist] is required while
UCRT is part of Windows 10 Operating System. You should run yass.exe without problems.

Below is a matrix support officially:
Architecture | Link | Notes
-- | -- | --
ARM64 | https://aka.ms/vs/17/release/vc_redist.arm64.exe | Permalink for latest supported ARM64 version
X86 | https://aka.ms/vs/17/release/vc_redist.x86.exe | Permalink for latest supported x86 version
X64 | https://aka.ms/vs/17/release/vc_redist.x64.exe | Permalink for latest supported x64 version. The X64 redistributable package contains both ARM64 and X64 binaries. This package makes it easy to install required Visual C++ ARM64 binaries when the X64 redistributable is installed on an ARM64 device.

### Notes on Vista, Windows 7, Windows 8 and Windows 8.1

If you run Windows prior to Windows 10, UCRT along with [Visual C++ Redistributable Package][latest-supported-vc-redist] is required:
Or you can install it via any of Monthly Quality Rollup, [KB3118401][KB3118401], or [KB2999226][KB2999226].

Below is a matrix support officially:
Architecture | Link | Notes
-- | -- | --
X86 | https://aka.ms/vs/17/release/vc_redist.x86.exe | Permalink for latest supported x86 version
X64 | https://aka.ms/vs/17/release/vc_redist.x64.exe | Permalink for latest supported x64 version. The X64 redistributable package contains both ARM64 and X64 binaries. This package makes it easy to install required Visual C++ ARM64 binaries when the X64 redistributable is installed on an ARM64 device.

### Notes on Windows XP

There is no standalone UCRT with Windows XP, so you should stick to the one
bundled inside Visual C++ Redistributable Package.

According to [Microsoft][latest-supported-vc-redist], the last version of the Visual C++ Redistributable that
works on Windows XP shipped in Visual Studio 2019 version 16.7 (file versions starting with 14.27).

You might want to download 14.28:
Architecture | Link | Notes
-- | -- | --
ARM64 | [14.28.VC_redist.arm64.exe] | 14.28.29213.0 - last version compatible with Windows XP
X86 | [14.28.VC_redist.x86.exe] | 14.28.29213.0 - last version compatible with Windows XP
X64 | [14.28.VC_redist.x64.exe] | 14.28.29213.0 - last version compatible with Windows XP. The X64 redistributable package contains both ARM64 and X64 binaries. This package makes it easy to install required Visual C++ ARM64 binaries when the X64 redistributable is installed on an ARM64 device.

Or more official 14.27:
Architecture | Link | Notes
-- | -- | --
ARM64 | [14.27.VC_redist.arm64.exe] | 14.27.29016.0 - last version officially compatible with Windows XP
X86 | [14.27.VC_redist.x86.exe] | 14.27.29016.0 - last version officially compatible with Windows XP
X64 | [14.27.VC_redist.x64.exe] | 14.27.29016.0 - last version officially compatible with Windows XP. The X64 redistributable package contains both ARM64 and X64 binaries. This package makes it easy to install required Visual C++ ARM64 binaries when the X64 redistributable is installed on an ARM64 device.

If you are looking for Visual Studio 2015 Runtime instead, here is the latest one (14.10.24516.0):
Architecture | Link | Notes
-- | -- | --
X86 | [14.10.VC_redist.x86.exe] | 14.10.24516.0 - last version of vc2015
X64 | [14.10.VC_redist.x64.exe] | 14.10.24516.0 - last version of vc2015

Reference: AIO Repack for latest Microsoft Visual C++ Redistributable Runtimes, without the original setup bloat payload.
- VC++ 2019 version 14.28.29213.0 = [VisualCppRedist_AIO v0.35.0][visual-cpp-redist-aio-xp] is the last version compatible with Windows XP

## Running on Ubuntu

Somehow if you manage to run Ubuntu 20.04 i386 (aka 32-bit) version, you might be
required by broken dependency libgtkmm-3.0. You might want to do either of the following:
- Build [gtkmm-3.0] from source
- Download and install prebuilt gtkmm-3.0 from [here][libgtkmm-3.0-1v5_3.24.2]
- Download and install prebuilt gtkmm-3.0 from previous distribution [here][libgtkmm-3.0-1v5_3.22.2]

[license-svg]: https://img.shields.io/badge/license-GPL2-lightgrey.svg
[license-link]: LICENSE

[aead]: https://shadowsocks.org/en/wiki/AEAD-Ciphers.html
[asan]: https://github.com/google/sanitizers/wiki/AddressSanitizer
[vcredist]: https://support.microsoft.com/zh-tw/help/2977003/the-latest-supported-visual-c-downloads
[aes128gcm]: https://tools.ietf.org/html/rfc5116
[aes128gcm12]: https://tools.ietf.org/html/rfc5282
[aes192gcm]: https://tools.ietf.org/html/rfc55282
[aes256gcm]: https://tools.ietf.org/html/rfc5116
[chacha20]: https://tools.ietf.org/html/rfc7539
[tls13]: https://tools.ietf.org/html/rfc7905

[latest-supported-vc-redist]: https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170
[KB3118401]: http://support.microsoft.com/kb/3118401
[KB2999226]: http://support.microsoft.com/kb/2999226
[ucrt]: https://www.microsoft.com/en-us/download/details.aspx?id=48234

[14.28.VC_redist.x64.exe]: https://download.visualstudio.microsoft.com/download/pr/566435ac-4e1c-434b-b93f-aecc71e8cffc/B75590149FA14B37997C35724BC93776F67E08BFF9BD5A69FACBF41B3846D084/VC_redist.x64.exe
[14.28.VC_redist.x86.exe]: https://download.visualstudio.microsoft.com/download/pr/566435ac-4e1c-434b-b93f-aecc71e8cffc/0D59EC7FDBF05DE813736BF875CEA5C894FFF4769F60E32E87BD48406BBF0A3A/VC_redist.x86.exe
[14.28.VC_redist.arm64.exe]: https://download.visualstudio.microsoft.com/download/pr/7b0dbd13-8740-4bcd-b86e-dffe0002c0b2/07C0219A8002491F85604EB76AADBD11DB819AF8A813621376B5DA5630C21E20/VC_redist.arm64.exe
[14.27.VC_redist.x64.exe]: https://download.visualstudio.microsoft.com/download/pr/fd5d2eea-32b8-4814-b55e-28c83dd72d9c/952A0C6CB4A3DD14C3666EF05BB1982C5FF7F87B7103C2BA896354F00651E358/VC_redist.x64.exe
[14.27.VC_redist.x86.exe]: https://download.visualstudio.microsoft.com/download/pr/cf2cc5ea-1976-4451-b226-e86508914f0f/B4D433E2F66B30B478C0D080CCD5217CA2A963C16E90CAF10B1E0592B7D8D519/VC_redist.x86.exe
[14.27.VC_redist.arm64.exe]: https://download.visualstudio.microsoft.com/download/pr/fd5d2eea-32b8-4814-b55e-28c83dd72d9c/95D3E19C9BDE8F0E8C0C73BF539CD2C62598498436FA896B864ECB8E3B70BD17/VC_redist.arm64.exe
[14.10.VC_redist.x64.exe]: http://download.microsoft.com/download/8/C/4/8C46752E-F6FD-43E4-AF10-E046A128CC0A/VC_redist.x64.exe
[14.10.VC_redist.x86.exe]: http://download.microsoft.com/download/0/5/2/05271FE6-CBA8-4A4D-9E95-00CFC60C1639/VC_redist.x86.exe
[visual-cpp-redist-aio-xp]: https://github.com/abbodi1406/vcredist/releases/tag/v0.35.0

[BUILDING.md]: BUILDING.md

[gtkmm-3.0]: https://packages.ubuntu.com/source/bionic/gtkmm3.0
[libgtkmm-3.0-1v5_3.24.2]: https://github.com/Chilledheart/yass/releases/download/r692/libgtkmm-3.0-1v5_3.24.2-1build1_i386.deb
[libgtkmm-3.0-1v5_3.22.2]: http://archive.ubuntu.com/ubuntu/pool/main/g/gtkmm3.0/libgtkmm-3.0-dev_3.22.2-2_i386.deb
