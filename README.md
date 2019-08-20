# Yet Another Shadow Socket

YASS(Yet-Another-Shadow-Socket) acts as a modern cplusplus port of the existing [shadowsocks](http://github.com/shadowsocks) project.

For now, it is heavily developed under macOS.

## History
Originally, ss is written to deal with TCP Relay Proxy and provide the similar functionality with openssh's builtin SOCKS proxy (provided with '-D' option).

It is not so good to stay as "SOCKS" (meaning silly rocket some how), but at most times programs are silly designed. Another port would make it stronger, so here we come with "Yet-Another-Shadow-Socket".

## Benefit
- Better scale, less memory consumption and CPU cycles, less than 0.1% on my iMac.
- Easier to maintain and adopt new features, so far [aead][aead] ciphers supported.
- Safe memory layout, tested against [Address Sanitizer][asan].

## License
It is dual-licensed with GPLv2 and Boost Software License.

## Support
### Installation
1. Make sure you have Xcode Command Line Tools installed (Xcode if possible):
   ```xcode-select --install```

2. Install [MacPorts] and ...
```
    cmake
    boost
    libglog
    gflags
    libsodium
    mbedtls
    jsoncpp
```
3. run
```yass_client --configfile <path-to-shadowsocks-json>```

### cipherers
- chacha20-ietf-poly1305
- xchacha20-ietf-poly1305
- salsa20
- chacha20
- chacha20-ietf

### Operating System
- macOS Catalina
- Linux
- Windows (possible)

[MacPorts]: https://www.macports.org/install.php
[aead]: https://shadowsocks.org/en/spec/AEAD-Ciphers.html
[asan]: https://github.com/google/sanitizers/wiki/AddressSanitizer
