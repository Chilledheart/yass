# Yet Another Shadow Socket

## Once upon a time, there are (virtual) Nics, so are (shadow) Sockets.

yass, or Yet Another Shadow Socket is lightweight and secure http/socks4/socks5 proxy.

## Benefit
- Better scale, less memory consumption and CPU cycles, less than 0.1% on my iMac.
- Easier to maintain and adopt new features, so far [aead][aead] ciphers supported.
- Safe memory layout, tested against [Address Sanitizer][asan].

## Supported Crypto Ciphers
### Ciphers for FIPS modules
- [AEAD_AES_128_GCM][aes128gcm]
- [AEAD_AES_256_GCM][aes256gcm]
- [AEAD_AES_128_GCM_12][aes128gcm12]
- [AEAD_AES_192_GCM][aes192gcm]

### More mobile-friendly ciphers [TLS 1.3][tls13]
- [AEAD_CHACHA20_POLY1305][chacha20]
- [AEAD_XCHACHA20_POLY1305][chacha20]

[![License][license-svg]][license-link]

[![Windows Build](https://github.com/Chilledheart/yass/actions/workflows/windows.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/windows.yml)
[![Linux Build](https://github.com/Chilledheart/yass/actions/workflows/linux.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/linux.yml)
[![macOS Build](https://github.com/Chilledheart/yass/actions/workflows/macos.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/macos.yml)

### Operating System
- macOS (MacOS 10.10 or later, macOS 11.0 or later, Apple Silicon supported)
- Linux (CentOS 8 or later, Debian 9 or later, Ubuntu 16.04 or later)
- Windows (Windows Vista or later, Windows XP standalone supported)

## Windows 7
<img width="484" alt="snapshot-win7" src="https://user-images.githubusercontent.com/54673341/69158208-02fa3600-0b21-11ea-9bf4-c38e26ff38a3.png">

## Windows 10 on ARM64
<img width="529" alt="Screen Shot 2020-12-05 at 6 49 32 PM" src="https://user-images.githubusercontent.com/54673341/101240623-715d6680-372b-11eb-9124-43817adb96a2.png">

## Notes on Windows 7, Windows 8 and Windows 8.1

If you run Windows prior to Windows 10, Visual C++ Runtime Files may be [required][latest-supported-vc-redist]:

Below is a matrix support officially:
Architecture | Link | Notes
-- | -- | --
ARM64 | https://aka.ms/vs/17/release/vc_redist.arm64.exe | Permalink for latest supported ARM64 version
X86 | https://aka.ms/vs/17/release/vc_redist.x86.exe | Permalink for latest supported x86 version
X64 | https://aka.ms/vs/17/release/vc_redist.x64.exe | Permalink for latest supported x64 version. The X64 redistributable package contains both ARM64 and X64 binaries. This package makes it easy to install required Visual C++ ARM64 binaries when the X64 redistributable is installed on an ARM64 device.

## Notes on Windows XP

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

### Client Protocols supported
- Socks4
- Socks4A
- Socks5
- HTTP/HTTPS

## License
It is licensed with GPLv2.

[aead]: https://shadowsocks.org/en/spec/AEAD-Ciphers.html
[asan]: https://github.com/google/sanitizers/wiki/AddressSanitizer
[vcredist]: https://support.microsoft.com/zh-tw/help/2977003/the-latest-supported-visual-c-downloads
[aes128gcm]: https://tools.ietf.org/html/rfc5116
[aes128gcm12]: https://tools.ietf.org/html/rfc5282
[aes192gcm]: https://tools.ietf.org/html/rfc55282
[aes256gcm]: https://tools.ietf.org/html/rfc5116
[chacha20]: https://tools.ietf.org/html/rfc7539
[tls13]: https://tools.ietf.org/html/rfc7905

[license-svg]: https://img.shields.io/badge/license-GPL2-lightgrey.svg
[license-link]: https://github.com/Chilledheart/yass/blob/master/COPYING

[latest-supported-vc-redist]: https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170
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
