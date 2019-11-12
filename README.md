# Yet Another Shadow Socket

## Once upon a time, there are (virtual) Nics, so are (shadow) Sockets.

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

## Build
### GeneralInstallation/Ubuntu
1. Install GNU C++ Compiler:
```
apt-get install -y build-essential
```
2. Install below dependencies:
```
apt-get install -y \
    cmake \
    ninja-build \
    libjemalloc-dev \
    libboost-all-dev \
    libgoogle-glog-dev \
    libgflags-dev \
    libsodium-dev \
    libssl-dev \
    libjsoncpp-dev \
    libwxgtk3.0-dev (optional)

```
3. compile the program with default configuration.
```
mkdir build
cd build
cmake -G Ninja..
ninja
```
4. run
```
./build/yass_cli
```

### macOS
1. Make sure you have Xcode Command Line Tools installed (Xcode if possible):
   ```xcode-select --install```

2. Install [MacPorts] and dependencies...
```
    boost
    cmake
    google-glog
    gflags
    libsodium
    jsoncpp
    jemalloc
    openssl
    wxWidgets-3.2
```
3. use script to build release App under `build` directory.
```scripts/build.py```

4. copy `build/yass.app` into `/Application` directory.
### vcpkg
```
vcpkg install boost-asio:x86-windows-static boost-filesystem:x86-windows-static boost-system:x86-windows-static glog:x86-windows-static gflags:x86-windows-static libsodium:x86-windows-static jsoncpp:x86-windows-static openssl:x86-windows-static wxwidgets:x86-windows-static jemalloc
```

## Crypto
### message digest
- md5 (used in stream)
- sha1+hmac (used in aead)
### ciphers
- chacha20-ietf-poly1305
- xchacha20-ietf-poly1305
- salsa20
- chacha20
- chacha20-ietf

### Operating System
- macOS
- Linux
- Windows

### Protocols supported
- Socks4
- Socks4A
- Socks5
- HTTP/HTTPS

[MacPorts]: https://www.macports.org/install.php
[aead]: https://shadowsocks.org/en/spec/AEAD-Ciphers.html
[asan]: https://github.com/google/sanitizers/wiki/AddressSanitizer
[vcredist]: https://support.microsoft.com/zh-tw/help/2977003/the-latest-supported-visual-c-downloads
