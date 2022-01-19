# Yet Another Shadow Socket

[![License][license-svg]][license-link]

[![Build Artifacts](https://github.com/Chilledheart/yass/actions/workflows/build.yml/badge.svg)](https://github.com/Chilledheart/yass/actions/workflows/build.yml)

## Windows 7
<img width="484" alt="snapshot-win7" src="https://user-images.githubusercontent.com/54673341/69158208-02fa3600-0b21-11ea-9bf4-c38e26ff38a3.png">

## Windows 10 on ARM64
<img width="529" alt="Screen Shot 2020-12-05 at 6 49 32 PM" src="https://user-images.githubusercontent.com/54673341/101240623-715d6680-372b-11eb-9124-43817adb96a2.png">

## Once upon a time, there are (virtual) Nics, so are (shadow) Sockets.

YASS(Yet Another Shadow Socket) acts as a modern cplusplus port of the existing [shadowsocks](http://github.com/shadowsocks) project.

For now, it is heavily developed under macOS.

## History
Originally, ss is written to deal with TCP Relay Proxy and provide the similar functionality with openssh's builtin SOCKS proxy (provided with '-D' option).

It is not so good to stay as "SOCKS" (meaning silly rocket some how), but at most times programs are silly designed. Another port would make it stronger, so here we come with "YetAnotherShadowSocket".

## Benefit
- Better scale, less memory consumption and CPU cycles, less than 0.1% on my iMac.
- Easier to maintain and adopt new features, so far [aead][aead] ciphers supported.
- Safe memory layout, tested against [Address Sanitizer][asan].

## License
It is licensed with GPLv2.

## Crypto
### Ciphers for fips modules
- [AEAD_AES_128_GCM][aes128gcm]
- [AEAD_AES_256_GCM][aes256gcm]
- [AEAD_AES_128_GCM_12][aes128gcm12]
- [AEAD_AES_192_GCM][aes192gcm]

### Ciphers for [tls 1.3][tls13]
- [AEAD_CHACHA20_POLY1305][chacha20]
- [AEAD_XCHACHA20_POLY1305][chacha20]

### Operating System
- macOS
- Linux
- Windows

## Note on Windows

If you run Windows prior to Windows 10, Visual C++ Runtime Files may be [required][latest-supported-vc-redist]:

Architecture | Link | Notes
-- | -- | --
ARM64 | https://aka.ms/vs/17/release/vc_redist.arm64.exe | Permalink for latest supported ARM64 version
X86 | https://aka.ms/vs/17/release/vc_redist.x86.exe | Permalink for latest supported x86 version
X64 | https://aka.ms/vs/17/release/vc_redist.x64.exe | Permalink for latest supported x64 version. The X64 redistributable package contains both ARM64 and X64 binaries. This package makes it easy to install required Visual C++ ARM64 binaries when the X64 redistributable is installed on an ARM64 device.

To download the universal CRT for central deployment on supported versions of Windows, see [Windows 10 Universal C Runtime][ucrt]:

https://www.microsoft.com/en-us/download/details.aspx?id=48234

### Client Protocols supported
- Socks4
- Socks4A
- Socks5
- HTTP/HTTPS

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
